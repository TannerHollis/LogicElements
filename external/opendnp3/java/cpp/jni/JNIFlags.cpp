//
//  _   _         ______    _ _ _   _             _ _ _
// | \ | |       |  ____|  | (_) | (_)           | | | |
// |  \| | ___   | |__   __| |_| |_ _ _ __   __ _| | | |
// | . ` |/ _ \  |  __| / _` | | __| | '_ \ / _` | | | |
// | |\  | (_) | | |___| (_| | | |_| | | | | (_| |_|_|_|
// |_| \_|\___/  |______\__,_|_|\__|_|_| |_|\__, (_|_|_)
//                                           __/ |
//                                          |___/
// 
// This file is auto-generated. Do not edit manually
// 
// Copyright 2013-2022 Step Function I/O, LLC
// 
// Licensed to Green Energy Corp (www.greenenergycorp.com) and Step Function I/O
// LLC (https://stepfunc.io) under one or more contributor license agreements.
// See the NOTICE file distributed with this work for additional information
// regarding copyright ownership. Green Energy Corp and Step Function I/O LLC license
// this file to you under the Apache License, Version 2.0 (the "License"); you
// may not use this file except in compliance with the License. You may obtain
// a copy of the License at:
// 
//   http://www.apache.org/licenses/LICENSE-2.0
// 
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//

#include "JNIFlags.h"

namespace jni
{
    namespace cache
    {
        bool Flags::init(JNIEnv* env)
        {
            auto clazzTemp = env->FindClass("Lcom/automatak/dnp3/Flags;");
            if(!clazzTemp) return false;
            this->clazz = (jclass) env->NewGlobalRef(clazzTemp);
            env->DeleteLocalRef(clazzTemp);

            this->constructor0 = env->GetMethodID(this->clazz, "<init>", "()V");
            if(!this->constructor0) return false;

            this->constructor1 = env->GetMethodID(this->clazz, "<init>", "(B)V");
            if(!this->constructor1) return false;

            return true;
        }

        void Flags::cleanup(JNIEnv* env)
        {
            env->DeleteGlobalRef(this->clazz);
        }

        LocalRef<JFlags> Flags::construct(JNIEnv* env)
        {
            return LocalRef<JFlags>(env, JFlags(env->NewObject(this->clazz, this->constructor0)));
        }

        LocalRef<JFlags> Flags::construct(JNIEnv* env, jbyte arg0)
        {
            return LocalRef<JFlags>(env, JFlags(env->NewObject(this->clazz, this->constructor1, arg0)));
        }
    }
}
