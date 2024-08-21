/*
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
#ifndef OPENDNP3_RESPONSECONTEXT_H
#define OPENDNP3_RESPONSECONTEXT_H

#include "app/AppControlField.h"
#include "app/HeaderWriter.h"
#include "app/Range.h"
#include "outstation/IResponseLoader.h"

#include "opendnp3/util/Uncopyable.h"

namespace opendnp3
{

class Database;

/**
 *  Coordinates the event & static response contexts to do multi-fragments responses
 */
class ResponseContext : private Uncopyable
{

public:
    ResponseContext(IResponseLoader& staticLoader, IResponseLoader& eventLoader);

    bool HasSelection() const;

    void Reset();

    AppControlField LoadResponse(HeaderWriter& writer);

private:
    static AppControlField GetControl(bool fir, bool fin, bool hasEvents);

    uint16_t fragmentCount;
    IResponseLoader* pStaticLoader;
    IResponseLoader* pEventLoader;
};

} // namespace opendnp3

#endif
