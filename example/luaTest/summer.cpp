﻿/*
 * zsummerX License
 * -----------
 * 
 * zsummerX is licensed under the terms of the MIT license reproduced below.
 * This means that zsummerX is free software and can be used for both academic
 * and commercial purposes at absolutely no cost.
 * 
 * 
 * ===============================================================================
 * 
 * Copyright (C) 2010-2015 YaweiZhang <yawei.zhang@foxmail.com>.
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 * 
 * ===============================================================================
 * 
 * (end of COPYRIGHT)
 */

#include "summer.h"

#include <zsummerX/zsummerX.h>
using namespace zsummer::proto4z;
using namespace zsummer::network;




static int logt(lua_State * L)
{
    const char * log = luaL_checkstring(L, 1);
    LOGT("from lua: " << log);
    return 0;
}
static int logd(lua_State * L)
{
    const char * log = luaL_checkstring(L, 1);
    LOGD("from lua: " << log);
    return 0;
}
static int logi(lua_State * L)
{
    const char * log = luaL_checkstring(L, 1);
    LOGI("from lua: " << log);
    return 0;
}
static int logw(lua_State * L)
{
    const char * log = luaL_checkstring(L, 1);
    LOGW("from lua: " << log);
    return 0;
}
static int loge(lua_State * L)
{
    const char * log = luaL_checkstring(L, 1);
    LOGE("from lua: " << log);
    return 0;
}
static int logf(lua_State * L)
{
    const char * log = luaL_checkstring(L, 1);
    LOGF("from lua: " << log);
    return 0;
}
static int loga(lua_State * L)
{
    const char * log = luaL_checkstring(L, 1);
    LOGA("from lua: " << log);
    return 0;
}

static int pcall_error(lua_State *L) 
{
    const char *msg = lua_tostring(L, 1);
    if (msg)
    {
        luaL_traceback(L, L, msg, 1);
    }
    else if (!lua_isnoneornil(L, 1)) 
    {  /* is there an error object? */
        if (!luaL_callmeta(L, 1, "__tostring"))  /* try its 'tostring' metamethod */
        {
            lua_pushliteral(L, "(no error message)");
        }
    }
    return 1;
}


//////////////////////////////////////////////////////////////////////////

static int _linkedRef = LUA_NOREF;
static int _messageRef = LUA_NOREF;
static int _closedRef = LUA_NOREF;
static int _pulseRef = LUA_NOREF;


static void _onSessionLinked(lua_State * L, TcpSessionPtr session)
{
    lua_pushcfunction(L, pcall_error);
    lua_rawgeti(L, LUA_REGISTRYINDEX, _linkedRef);
    lua_pushnumber(L, session->getSessionID());
    lua_pushstring(L, session->getRemoteIP().c_str());
    lua_pushnumber(L, session->getRemotePort());
    int status = lua_pcall(L, 3, 0, 1);
    lua_remove(L, 1);
    if (status)
    {
        const char *msg = lua_tostring(L, -1);
        if (msg == NULL) msg = "(error object is not a string)";
        LOGE(msg);
        lua_pop(L, 1);
    }
}

static void _onMessage(lua_State * L, TcpSessionPtr session, const char * begin, unsigned int len)
{
    ReadStream rs(begin, len);
    lua_pushcfunction(L, pcall_error);
    lua_rawgeti(L, LUA_REGISTRYINDEX, _messageRef);
    lua_pushinteger(L, session->getSessionID());
    lua_pushinteger(L, rs.getProtoID());
    lua_pushlstring(L, rs.getStreamBody(), rs.getStreamBodyLen());
    int status = lua_pcall(L, 3, 0, 1);
    lua_remove(L, 1);
    if (status)
    {
        const char *msg = lua_tostring(L, -1);
        if (msg == NULL) msg = "(error object is not a string)";
        LOGE(msg);
        lua_pop(L, 1);
    }
}

static void _onSessionClosed(lua_State * L, TcpSessionPtr session)
{
    if (_closedRef == LUA_NOREF)
    {
        LOGW("_onSessionClosed warning: cannot found ther callback.");
        return;
    }
    lua_pushcfunction(L, pcall_error);
    lua_rawgeti(L, LUA_REGISTRYINDEX, _closedRef);
    lua_pushnumber(L, session->getSessionID());
    lua_pushstring(L, session->getRemoteIP().c_str());
    lua_pushnumber(L, session->getRemotePort());
    int status = lua_pcall(L, 3, 0, 1);
    lua_remove(L, 1);
    if (status)
    {
        const char *msg = lua_tostring(L, -1);
        if (msg == NULL) msg = "(error object is not a string)";
        LOGE(msg);
        lua_pop(L, 1);
    }
}

static void _onSessionPulse(lua_State * L, TcpSessionPtr session)
{
    if (_pulseRef == LUA_NOREF)
    {
        LOGD("_onSessionPulse no callback.");
        return;
    }
    lua_pushcfunction(L, pcall_error);
    lua_rawgeti(L, LUA_REGISTRYINDEX, _pulseRef);
    lua_pushnumber(L, session->getSessionID());
    int status = lua_pcall(L, 1, 0, 1);
    lua_remove(L, 1);
    if (status)
    {
        const char *msg = lua_tostring(L, -1);
        if (msg == NULL) msg = "(error object is not a string)";
        LOGE(msg);
        lua_pop(L, 1);
    }
}

static void onStopServersFinish()
{
    SessionManager::getRef().stop();
}

static void onStopClientsFinish()
{
    SessionManager::getRef().stopServers();
}


static int start(lua_State *L)
{
    SessionManager::getRef().start();
    SessionManager::getRef().setStopClientsHandler(onStopClientsFinish);
    SessionManager::getRef().setStopServersHandler(onStopServersFinish);
    return 0;
}


static int stop(lua_State *L)
{
    SessionManager::getRef().stopAccept();
    SessionManager::getRef().stopClients();
    return 0;
}

static int runOnce(lua_State * L)
{
    int isImmediately = lua_toboolean(L, 1);
    bool ret = SessionManager::getRef().runOnce((bool)isImmediately);
    lua_pushboolean(L, (int)ret);
    return 1;
}

static int run(lua_State * L)
{
    while (SessionManager::getRef().runOnce());
    return 0;
}

static int addConnect(lua_State *L)
{
    SessionID cID = SessionManager::getRef().addConnecter(luaL_checkstring(L, 1), (unsigned short)luaL_checkinteger(L, 2));
    auto & traits = SessionManager::getRef().getConnecterOptions(cID);
    traits._rc4TcpEncryption = luaL_optstring(L, 3, traits._rc4TcpEncryption.c_str());
    traits._reconnects = (unsigned int)luaL_optinteger(L, 4, traits._reconnects);
    traits._connectPulseInterval = (unsigned int)luaL_optinteger(L, 5, traits._connectPulseInterval);
    traits._onSessionLinked = std::bind(_onSessionLinked, L, std::placeholders::_1);
    traits._onBlockDispatch = std::bind(_onMessage, L, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3);
    traits._onSessionClosed = std::bind(_onSessionClosed, L, std::placeholders::_1);
    traits._onSessionPulse = std::bind(_onSessionPulse, L, std::placeholders::_1);

    if (!SessionManager::getRef().openConnecter(cID))
    {
        return 0;
    }
    lua_pushnumber(L, cID);
    return 1;
}



static int addListen(lua_State *L)
{
    AccepterID aID = SessionManager::getRef().addAccepter(luaL_checkstring(L, 1), (unsigned short)luaL_checkinteger(L, 2));
    auto &extend = SessionManager::getRef().getAccepterOptions(aID);
    extend._sessionOptions._rc4TcpEncryption = luaL_optstring(L, 3, extend._sessionOptions._rc4TcpEncryption.c_str());
    extend._maxSessions = (unsigned int)luaL_optinteger(L, 4, extend._maxSessions);
    extend._sessionOptions._sessionPulseInterval = (unsigned int)luaL_optinteger(L, 5, extend._sessionOptions._sessionPulseInterval);
    extend._sessionOptions._onSessionLinked = std::bind(_onSessionLinked, L, std::placeholders::_1);
    extend._sessionOptions._onBlockDispatch = std::bind(_onMessage, L, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3);
    extend._sessionOptions._onSessionClosed = std::bind(_onSessionClosed, L, std::placeholders::_1);
    extend._sessionOptions._onSessionPulse = std::bind(_onSessionPulse, L, std::placeholders::_1);

    LOGD("lua: addListen:" << extend);

    if (!SessionManager::getRef().openAccepter(aID))
    {
        return 0;
    }
    lua_pushnumber(L, aID);
    return 1;
}




static int setLinkedRef(lua_State * L)
{
    luaL_checktype(L, 1, LUA_TFUNCTION);
    lua_settop(L, 1);
    if (_linkedRef != LUA_NOREF)
    {
        LOGE("setLinkedRef error. connect callback already .");
        return 0;
    }
    _linkedRef = luaL_ref(L, LUA_REGISTRYINDEX);
    return 0;
}




static int setMessageRef(lua_State * L)
{
    luaL_checktype(L, 1, LUA_TFUNCTION);
    lua_settop(L, 1);
    if (_messageRef != LUA_NOREF)
    {
        LOGE("setMessageRef error. connect callback already .");
        return 0;
    }
    _messageRef = luaL_ref(L, LUA_REGISTRYINDEX);
    return 0;
}





static int setClosedRef(lua_State * L)
{
    luaL_checktype(L, 1, LUA_TFUNCTION);
    lua_settop(L, 1);
    if (_closedRef != LUA_NOREF)
    {
        LOGE("setClosedRef error. connect callback already .");
        return 0;
    }

    _closedRef = luaL_ref(L, LUA_REGISTRYINDEX);
    return 0;
}

static int setPulseRef(lua_State * L)
{
    luaL_checktype(L, 1, LUA_TFUNCTION);
    lua_settop(L, 1);
    if (_pulseRef != LUA_NOREF)
    {
        LOGE("setClosedRef error. connect callback already .");
        return 0;
    }

    _pulseRef = luaL_ref(L, LUA_REGISTRYINDEX);
    return 0;
}


static int sendContent(lua_State * L)
{
    SessionID sID = (SessionID)luaL_checkinteger(L, 1);
    zsummer::proto4z::ProtoInteger pID = (zsummer::proto4z::ProtoInteger)luaL_checkinteger(L, 2);
    size_t len = 0;
    const char * content = luaL_checklstring(L, 3, &len);
    WriteStream ws(pID);
    ws.appendOriginalData(content, (zsummer::proto4z::Integer)len);
    SessionManager::getRef().sendSessionData(sID, ws.getStream(), ws.getStreamLen());
    return 0;
}

static int sendData(lua_State * L)
{
    SessionID sID = (SessionID)luaL_checkinteger(L, 1);
    size_t len = 0;
    const char * data = luaL_checklstring(L, 2, &len);
    SessionManager::getRef().sendSessionData(sID, data, (unsigned short)len);
    return 0;
}

static int kick(lua_State * L)
{
    SessionID sID = (SessionID)luaL_checkinteger(L, 1);
    SessionManager::getRef().kickSession(sID);
    return 0;
}

static void onPost(lua_State * L, int delay, int refID)
{
    lua_pushcfunction(L, pcall_error);
    lua_rawgeti(L, LUA_REGISTRYINDEX, refID);
    int status = lua_pcall(L, 0, 0, 1);
    lua_remove(L, 1);
    if (status)
    {
        const char *msg = lua_tostring(L, -1);
        if (msg == NULL) msg = "(error object is not a string)";
        LOGE(msg);
        lua_pop(L, 1);
    }
    luaL_unref(L, LUA_REGISTRYINDEX, refID);
}

static int _post(lua_State * L)
{
    int delay = (int)luaL_checkinteger(L, 1);
    luaL_checktype(L, -1, LUA_TFUNCTION);
    int ret = luaL_ref(L, LUA_REGISTRYINDEX);
    if (delay <= 0)
    {
        SessionManager::getRef().post(std::bind(onPost, L, delay, ret));
    }
    else
    {
        SessionManager::getRef().createTimer(delay, std::bind(onPost, L, delay, ret));
    }
    return 0;
}

static luaL_Reg summer[] = {
    { "logd", logd },
    { "logi", logi },
    { "logt", logt },
    { "logw", logw },
    { "loge", loge },
    { "logf", logf },
    { "loga", loga },

    { "start", start }, //start network
    { "stop", stop }, //stop network
    { "run", run }, //run
    { "runOnce", runOnce }, //message pump, run it once.
    { "addConnect", addConnect }, //add one connect.
    { "addListen", addListen }, //add listen.
    { "whenLinked", setLinkedRef }, //register event when connect success.
    { "whenMessage", setMessageRef }, //register event when recv message.
    { "whenClosed", setClosedRef }, //register event when disconnect.
    { "whenPulse", setPulseRef }, //register event when pulse.
    { "sendContent", sendContent }, //send content, don't care serialize and package.
    { "sendData", sendData }, // send original data, need to serialize and package via proto4z. 
    { "kick", kick }, // kick session. 
    { "post", _post }, // kick session. 

    { NULL, NULL }
};


int luaopen_summer(lua_State *L)
{
    lua_newtable(L);
    for (luaL_Reg *l = summer; l->name != NULL; l++) {  
        lua_pushcclosure(L, l->func, 0);  /* closure with those upvalues */
        lua_setfield(L, -2, l->name);
    }
    lua_setglobal(L, "summer");
    return 0;
}



