// Copyright 2020 The Defold Foundation
// Licensed under the Defold License version 1.0 (the "License"); you may not use
// this file except in compliance with the License.
//
// You may obtain a copy of the License, together with FAQs at
// https://www.defold.com/license
//
// Unless required by applicable law or agreed to in writing, software distributed
// under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
// CONDITIONS OF ANY KIND, either express or implied. See the License for the
// specific language governing permissions and limitations under the License.

#include <dmsdk/lua/lua.h>

namespace dmRive
{

void    RegisterUserType(lua_State* L, const char* class_name, const char* metatable_name, const luaL_reg methods[], const luaL_reg meta[]);
void*   ToUserType(lua_State* L, int user_data_index, const char* metatable_name);
void*   CheckUserType(lua_State* L, int user_data_index, const char* metatable_name);

// Helper to call methods from within an __index function
int     PushFromMetaMethods(lua_State* L, int obj_index, int key_index);
}
