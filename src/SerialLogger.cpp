// Copyright (c) Microsoft Corporation. All rights reserved.
// SPDX-License-Identifier: MIT

#include "SerialLogger.h"

SerialLogger::SerialLogger() { }

void SerialLogger::Info(String message)
{
#if SERIAL_LOGGER_LEVEL >= 2
  Serial.print("[INFO] ");
  Serial.println(message);
#endif
}

void SerialLogger::Verbose(String message)
{
#if SERIAL_LOGGER_LEVEL >= 3
  Serial.print("[VERBOSE] ");
  Serial.println(message);
#endif
}

void SerialLogger::Error(String message)
{
#if SERIAL_LOGGER_LEVEL >= 1
  Serial.print("[ERROR] ");
  Serial.println(message);
#endif
}

SerialLogger Logger;