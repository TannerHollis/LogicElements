/**
 * Copyright 2013-2022 Step Function I/O, LLC
 *
 * Licensed to Green Energy Corp (www.greenenergycorp.com) and Step Function I/O
 * LLC (https://stepfunc.io) under one or more contributor license agreements.
 * See the NOTICE file distributed with this work for additional information
 * regarding copyright ownership. Green Energy Corp and Step Function I/O LLC license
 * this file to you under the Apache License, Version 2.0 (the "License"); you
 * may not use this file except in compliance with the License. You may obtain
 * a copy of the License at:
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
package com.automatak.render.dnp3.enums

import com.automatak.render._

object RestartMode {

  private val comments = List("Enumeration describing restart mode support of an outstation")

  def apply(): EnumModel = EnumModel("RestartMode", comments, EnumModel.UInt8, codes, None, Base10)

  private val codes = List(
    EnumValue("UNSUPPORTED", 0, "Device does not support restart"),
    EnumValue("SUPPORTED_DELAY_FINE", 1, "Supports restart, and time returned is a fine time delay"),
    EnumValue("SUPPORTED_DELAY_COARSE", 2, "Supports restart, and time returned is a coarse time delay")
  )

}


