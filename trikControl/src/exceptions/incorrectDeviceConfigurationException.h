/* Copyright 2015 CyberTech Labs Ltd.
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

#pragma once

#include <trikKernel/exceptions/trikRuntimeException.h>

#include <QsLog.h>

namespace trikControl {

/// Exception for semantically incorrect device config, for example, trying to init MxN color sensor with negative
/// grid size.
class IncorrectDeviceConfigurationException : public trikKernel::TrikRuntimeException
{
public:
	/// Constructor.
	/// @param message - message with some information about exception.
	IncorrectDeviceConfigurationException(QString const &message)
	{
		QLOG_ERROR() << message;
	}
};

}