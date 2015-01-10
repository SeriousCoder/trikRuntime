/* Copyright 2013 - 2015 Yurii Litvinov and CyberTech Labs Ltd.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License. */

#include "brick.h"

#include <QtCore/QDebug>
#include <QtCore/QDateTime>
#include <QtCore/QCoreApplication>
#include <QtCore/QProcess>
#include <QtCore/QFileInfo>

#include "analogSensor.h"
#include "angularServoMotor.h"
#include "continiousRotationServoMotor.h"
#include "display.h"
#include "powerMotor.h"
#include "digitalSensor.h"
#include "rangeSensor.h"
#include "pwmCapture.h"
#include "encoder.h"
#include "battery.h"
#include "vectorSensor.h"
#include "keys.h"
#include "led.h"
#include "gamepad.h"
#include "lineSensor.h"
#include "objectSensor.h"
#include "colorSensor.h"
#include "mailbox.h"

#include "configurer.h"
#include "i2cCommunicator.h"

#include <QsLog.h>

using namespace trikControl;

Brick::Brick(QThread &guiThread, QString const &configFilePath, const QString &startDirPath)
	: mConfigurer(new Configurer(configFilePath))
	, mI2cCommunicator(nullptr)
	, mDisplay(new Display(guiThread, startDirPath))
{
	qRegisterMetaType<QVector<int>>("QVector<int>");

	if (::system(mConfigurer->initScript().toStdString().c_str()) != 0) {
		QString const message = "Init script failed";
		QLOG_ERROR() << message;
		qDebug() << message;
	}

	mI2cCommunicator = new I2cCommunicator(mConfigurer->i2cPath(), mConfigurer->i2cDeviceId());

	for (QString const &port : mConfigurer->servoMotorPorts()) {
		QString const servoMotorType = mConfigurer->servoMotorDefaultType(port);

		ServoMotor *servoMotor = nullptr;
		if (mConfigurer->isServoMotorTypeContiniousRotation(servoMotorType)) {
			servoMotor = new ContiniousRotationServoMotor(
					mConfigurer->servoMotorTypeMin(servoMotorType)
					, mConfigurer->servoMotorTypeMax(servoMotorType)
					, mConfigurer->servoMotorTypeZero(servoMotorType)
					, mConfigurer->servoMotorTypeStop(servoMotorType)
					, mConfigurer->servoMotorDeviceFile(port)
					, mConfigurer->servoMotorPeriodFile(port)
					, mConfigurer->servoMotorPeriod(port)
					, mConfigurer->servoMotorInvert(port)
					);
		} else {
			servoMotor = new AngularServoMotor(
					mConfigurer->servoMotorTypeMin(servoMotorType)
					, mConfigurer->servoMotorTypeMax(servoMotorType)
					, mConfigurer->servoMotorTypeZero(servoMotorType)
					, mConfigurer->servoMotorTypeStop(servoMotorType)
					, mConfigurer->servoMotorDeviceFile(port)
					, mConfigurer->servoMotorPeriodFile(port)
					, mConfigurer->servoMotorPeriod(port)
					, mConfigurer->servoMotorInvert(port)
					);
		}

		mServoMotors.insert(port, servoMotor);
	}

	for (QString const &port : mConfigurer->pwmCapturePorts()) {
		PwmCapture *pwmCapture = new PwmCapture(
				mConfigurer->pwmCaptureFrequencyFile(port)
				, mConfigurer->pwmCaptureDutyFile(port)
				);

		mPwmCaptures.insert(port, pwmCapture);
	}

	for (QString const &port : mConfigurer->powerMotorPorts()) {
		PowerMotor *powerMotor = new PowerMotor(
				*mI2cCommunicator
				, mConfigurer->powerMotorI2cCommandNumber(port)
				, mConfigurer->powerMotorInvert(port)
				);

		mPowerMotors.insert(port, powerMotor);
	}

	for (QString const &port : mConfigurer->analogSensorPorts()) {
		QString const analogSensorType = mConfigurer->analogSensorDefaultType(port);

		AnalogSensor *analogSensor = new AnalogSensor(
			*mI2cCommunicator
			, mConfigurer->analogSensorI2cCommandNumber(port)
			, mConfigurer->analogSensorTypeRawValue1(analogSensorType)
			, mConfigurer->analogSensorTypeRawValue2(analogSensorType)
			, mConfigurer->analogSensorTypeNormalizedValue1(analogSensorType)
			, mConfigurer->analogSensorTypeNormalizedValue2(analogSensorType)
			);

		mAnalogSensors.insert(port, analogSensor);
	}

	for (QString const &port : mConfigurer->digitalSensorPorts()) {
		QString const digitalSensorType = mConfigurer->digitalSensorDefaultType(port);

		DigitalSensor *digitalSensor = new DigitalSensor(
				mConfigurer->digitalSensorTypeMin(digitalSensorType)
				, mConfigurer->digitalSensorTypeMax(digitalSensorType)
				, mConfigurer->digitalSensorDeviceFile(port)
				);

		mDigitalSensors.insert(port, digitalSensor);
	}

	for (QString const &port : mConfigurer->rangeSensorPorts()) {
		RangeSensor *rangeSensor = new RangeSensor(mConfigurer->rangeSensorEventFile(port));
		rangeSensor->init();
		mRangeSensors.insert(port, rangeSensor);
	}

	for (QString const &port : mConfigurer->encoderPorts()) {
		QString const encoderType = mConfigurer->encoderDefaultType(port);

		Encoder *encoder = new Encoder(
				*mI2cCommunicator
				, mConfigurer->encoderI2cCommandNumber(port)
				, mConfigurer->encoderTypeRawToDegrees(encoderType));
		mEncoders.insert(port, encoder);
	}

	mBattery = new Battery(*mI2cCommunicator);

	if (mConfigurer->hasAccelerometer()) {
		mAccelerometer = new VectorSensor(mConfigurer->accelerometerDeviceFile());
	}

	if (mConfigurer->hasGyroscope()) {
		mGyroscope = new VectorSensor(mConfigurer->gyroscopeDeviceFile());
	}

	mKeys = new Keys(mConfigurer->keysDeviceFile());

	mLed = new Led(mConfigurer->ledRedDeviceFile()
			, mConfigurer->ledGreenDeviceFile()
			, mConfigurer->ledOn()
			, mConfigurer->ledOff()
			);

	if (mConfigurer->hasGamepad()) {
		mGamepad = new Gamepad(mConfigurer->gamepadPort());
	}

	if (mConfigurer->hasLineSensor()) {
		mLineSensor = new LineSensor(mConfigurer->lineSensorScript()
				, mConfigurer->lineSensorInFifo()
				, mConfigurer->lineSensorOutFifo()
				, mConfigurer->lineSensorToleranceFactor()
				);

		/// @todo This will work only in case when there can be only one video sensor launched at a time.
		connect(mLineSensor, SIGNAL(stopped()), this, SIGNAL(stopped()));
	}

	if (mConfigurer->hasObjectSensor()) {
		mObjectSensor = new ObjectSensor(mConfigurer->objectSensorScript()
				, mConfigurer->objectSensorInFifo()
				, mConfigurer->objectSensorOutFifo()
				, mConfigurer->objectSensorToleranceFactor()
				);

		/// @todo This will work only in case when there can be only one video sensor launched at a time.
		connect(mObjectSensor, SIGNAL(stopped()), this, SIGNAL(stopped()));
	}

	if (mConfigurer->hasColorSensor()) {
		mColorSensor = new ColorSensor(mConfigurer->colorSensorScript()
				, mConfigurer->colorSensorInFifo()
				, mConfigurer->colorSensorOutFifo()
				, mConfigurer->colorSensorM()
				, mConfigurer->colorSensorN()
				);

		/// @todo This will work only in case when there can be only one video sensor launched at a time.
		connect(mColorSensor, SIGNAL(stopped()), this, SIGNAL(stopped()));
	}

	if (mConfigurer->hasMailbox()) {
		mMailbox.reset(new Mailbox(mConfigurer->mailboxServerPort()));
		QObject::connect(this, SIGNAL(stopWaiting()), mMailbox.data(), SIGNAL(stopWaiting()));
	}
}

Brick::~Brick()
{
	delete mConfigurer;
	qDeleteAll(mServoMotors);
	qDeleteAll(mPwmCaptures);
	qDeleteAll(mPowerMotors);
	qDeleteAll(mEncoders);
	qDeleteAll(mAnalogSensors);
	qDeleteAll(mDigitalSensors);
	qDeleteAll(mRangeSensors);
	delete mAccelerometer;
	delete mGyroscope;
	delete mBattery;
	delete mI2cCommunicator;
	delete mLed;
	delete mKeys;
	delete mGamepad;
	delete mLineSensor;
	delete mColorSensor;
	delete mObjectSensor;
}

trikKernel::LazyMainWidget &Brick::graphicsWidget()
{
	return mDisplay->graphicsWidget();
}

void Brick::reset()
{
	stop();
	emit stopWaiting();
	mKeys->reset();
	mDisplay->clear();

	/// @todo Temporary, we need more carefully init/deinit range sensors.
	for (RangeSensor * const rangeSensor : mRangeSensors.values()) {
		rangeSensor->init();
	}
}

void Brick::playSound(QString const &soundFileName)
{
	QLOG_INFO() << "Playing " << soundFileName;
	qDebug() << soundFileName;

	QFileInfo const fileInfo(soundFileName);

	QString command;

	if (fileInfo.suffix() == "wav") {
		command = mConfigurer->playWavFileCommand().arg(soundFileName);
	} else if (fileInfo.suffix() == "mp3") {
		command = mConfigurer->playMp3FileCommand().arg(soundFileName);
	}

	if (command.isEmpty() || ::system(command.toStdString().c_str()) != 0) {
		QString const message = "Play sound failed";
		QLOG_ERROR() << message;
		qDebug() << message;
	}
}

void Brick::say(QString const &text)
{
	QStringList args{"-c", "espeak -v russian_test -s 100 \"" + text + "\""};
	QProcess::startDetached("sh", args);
}

void Brick::stop()
{
	QLOG_INFO() << "Stopping brick";

	for (ServoMotor * const servoMotor : mServoMotors.values()) {
		servoMotor->powerOff();
	}

	for (PowerMotor * const powerMotor : mPowerMotors.values()) {
		powerMotor->powerOff();
	}

	mLed->red();
	mDisplay->hide();

	if (mLineSensor) {
		mLineSensor->stop();
	}

	if (mColorSensor) {
		mColorSensor->stop();
	}

	if (mObjectSensor) {
		mObjectSensor->stop();
	}

	for (RangeSensor * const rangeSensor : mRangeSensors.values()) {
		rangeSensor->stop();
	}
}

MotorInterface *Brick::motor(QString const &port)
{
	if (mPowerMotors.contains(port)) {
		return mPowerMotors[port];
	} else if (mServoMotors.contains(port)) {
		return mServoMotors[port];
	} else {
		return nullptr;
	}
}

PwmCaptureInterface *Brick::pwmCapture(QString const &port)
{
	return mPwmCaptures.value(port, nullptr);
}

SensorInterface *Brick::sensor(QString const &port)
{
	if (mAnalogSensors.contains(port)) {
		return mAnalogSensors[port];
	} else if (mDigitalSensors.contains(port)) {
		return mDigitalSensors[port];
	} else if (mRangeSensors.contains(port)) {
		return mRangeSensors[port];
	} else {
		return nullptr;
	}
}

QStringList Brick::motorPorts(MotorInterface::Type type) const
{
	switch (type) {
	case MotorInterface::Type::powerMotor: {
		return mPowerMotors.keys();
	}
	case MotorInterface::Type::servoMotor: {
		return mServoMotors.keys();
	}
	}

	return QStringList();
}

QStringList Brick::pwmCapturePorts() const
{
	return mPwmCaptures.keys();
}

QStringList Brick::sensorPorts(SensorInterface::Type type) const
{
	switch (type) {
	case SensorInterface::Type::analogSensor: {
		return mAnalogSensors.keys();
	}
	case SensorInterface::Type::digitalSensor: {
		return mDigitalSensors.keys() + mRangeSensors.keys();
	}
	case SensorInterface::Type::specialSensor: {
		// Special sensors can not be connected to standard ports, they have their own methods to access them.
		return QStringList();
	}
	}

	return QStringList();
}

EncoderInterface *Brick::encoder(QString const &port)
{
	return mEncoders.value(port, nullptr);
}

BatteryInterface *Brick::battery()
{
	return mBattery;
}

VectorSensorInterface *Brick::accelerometer()
{
	return mAccelerometer;
}

VectorSensorInterface *Brick::gyroscope()
{
	return mGyroscope;
}

LineSensorInterface *Brick::lineSensor()
{
	return mLineSensor;
}

ColorSensorInterface *Brick::colorSensor()
{
	return mColorSensor;
}

ObjectSensorInterface *Brick::objectSensor()
{
	return mObjectSensor;
}

KeysInterface* Brick::keys()
{
	return mKeys;
}

QStringList Brick::encoderPorts() const
{
	return mEncoders.keys();
}

GamepadInterface* Brick::gamepad()
{
	return mGamepad;
}

DisplayInterface *Brick::display()
{
	return mDisplay.data();
}

LedInterface *Brick::led()
{
	return mLed;
}

MailboxInterface *Brick::mailbox()
{
	return mMailbox.data();
}
