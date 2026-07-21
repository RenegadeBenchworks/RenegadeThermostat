// Copyright (c) Microsoft Corporation. All rights reserved.
// SPDX-License-Identifier: MIT

#ifndef SERIALLOGGER_H
#define SERIALLOGGER_H

#include <Arduino.h>

#ifndef SERIAL_LOGGER_BAUD_RATE
#define SERIAL_LOGGER_BAUD_RATE 115200
#endif

// Log level controls:
// 0 = no logs, 1 = errors, 2 = info, 3 = verbose
#ifndef SERIAL_LOGGER_LEVEL
#define SERIAL_LOGGER_LEVEL 2
#endif

class SerialLogger
{
public:
  SerialLogger();
  void Info(String message);
  void Verbose(String message);
  void Error(String message);
};

extern SerialLogger Logger;

#endif // SERIALLOGGER_H