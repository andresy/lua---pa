#include <stdio.h>
#include <portaudio.h>
#include <luaT.h>
#include "TH.h"

const void* torch_ShortTensor_id;
const void* torch_IntTensor_id;
const void* torch_FloatTensor_id;
const void* torch_DoubleTensor_id;

static void pa_checkerror(lua_State *L, PaError err)
{
  if(err != paNoError)
  {
    Pa_Terminate();
    luaL_error(L, "An error occured while using the portaudio stream: %s", Pa_GetErrorText(err));
  }
}
/*
static const struct luaL_Reg sndfile_SndFile__ [] = {
  {NULL, NULL}
};
*/

static int pa_version(lua_State *L)
{
  int narg = lua_gettop(L);
  if(narg != 0)
    luaL_error(L, "invalid arguments: no argument expected");

  lua_pushstring(L, Pa_GetVersionText());
  return 1;
}

static int pa_hostapicount(lua_State *L)
{
  int narg = lua_gettop(L);
  if(narg != 0)
    luaL_error(L, "invalid arguments: no argument expected");

  lua_pushnumber(L, Pa_GetHostApiCount());
  return 1;
}

static int pa_defaulthostapi(lua_State *L)
{
  int narg = lua_gettop(L);
  if(narg != 0)
    luaL_error(L, "invalid arguments: no argument expected");

  lua_pushnumber(L, Pa_GetDefaultHostApi()+1);
  return 1;
}

static int pa_hostapiinfo(lua_State *L)
{
  int narg = lua_gettop(L);
  PaHostApiIndex apiidx = 0;
  const PaHostApiInfo *info;

  if(narg == 1 && lua_isnumber(L, 1))
    apiidx = (PaHostApiIndex)lua_tonumber(L, 1)-1;
  else
    luaL_error(L, "expected arguments: number");
  
  info = Pa_GetHostApiInfo(apiidx);
  if(info)
  {
    lua_newtable(L);
    lua_pushstring(L, info->name);
    lua_setfield(L, -2, "name");
    lua_pushnumber(L, info->deviceCount);
    lua_setfield(L, -2, "devicecount");
    lua_pushnumber(L, info->defaultInputDevice+1);
    lua_setfield(L, -2, "defaultinputdevice");
    lua_pushnumber(L, info->defaultOutputDevice+1);
    lua_setfield(L, -2, "defaultoutputdevice");
    return 1;
  }

  return 0;
}

static int pa_devicecount(lua_State *L)
{
  int narg = lua_gettop(L);
  if(narg != 0)
    luaL_error(L, "invalid arguments: no argument expected");

  lua_pushnumber(L, Pa_GetDeviceCount());
  return 1;
}

static int pa_defaultinputdevice(lua_State *L)
{
  int narg = lua_gettop(L);
  if(narg != 0)
    luaL_error(L, "invalid arguments: no argument expected");

  lua_pushnumber(L, Pa_GetDefaultInputDevice()+1);
  return 1;
}

static int pa_defaultoutputdevice(lua_State *L)
{
  int narg = lua_gettop(L);
  if(narg != 0)
    luaL_error(L, "invalid arguments: no argument expected");

  lua_pushnumber(L, Pa_GetDefaultOutputDevice()+1);
  return 1;
}

static int pa_deviceinfo(lua_State *L)
{
  int narg = lua_gettop(L);
  PaDeviceIndex devidx = 0;
  const PaDeviceInfo *info;

  if(narg == 1 && lua_isnumber(L, 1))
    devidx = (PaHostApiIndex)lua_tonumber(L, 1)-1;
  else
    luaL_error(L, "expected arguments: number");
  
  info = Pa_GetDeviceInfo(devidx);
  if(info)
  {
    lua_newtable(L);
    lua_pushstring(L, info->name);
    lua_setfield(L, -2, "name");
    lua_pushnumber(L, info->hostApi+1);
    lua_setfield(L, -2, "hostapi");
    lua_pushnumber(L, info->maxInputChannels);
    lua_setfield(L, -2, "maxinputchannels");
    lua_pushnumber(L, info->maxOutputChannels);
    lua_setfield(L, -2, "maxoutputchannels");
    lua_pushnumber(L, info->defaultLowInputLatency);
    lua_setfield(L, -2, "defaultlowinputlatency");
    lua_pushnumber(L, info->defaultLowOutputLatency);
    lua_setfield(L, -2, "defaultlowoutputlatency");
    lua_pushnumber(L, info->defaultHighInputLatency);
    lua_setfield(L, -2, "defaulthighinputlatency");
    lua_pushnumber(L, info->defaultHighOutputLatency);
    lua_setfield(L, -2, "defaulthighoutputlatency");
    lua_pushnumber(L, info->defaultSampleRate);
    lua_setfield(L, -2, "defaultsamplerate");
    return 1;
  }

  return 0;
}

static void pa_readstreamparameters(lua_State *L, int idx, PaStreamParameters *params)
{
  lua_getfield(L, idx, "device");
  if(!lua_isnumber(L, -1))
    luaL_error(L, "device field must be a number");
  params->device = (PaDeviceIndex)(lua_tonumber(L, -1)-1);
  lua_pop(L, 1);

  lua_getfield(L, idx, "channelcount");
  if(!lua_isnumber(L, -1))
    luaL_error(L, "channelcount field must be a number");
  params->channelCount = (int)(lua_tonumber(L, -1));
  lua_pop(L, 1);

  lua_getfield(L, idx, "sampleformat");
  if(!lua_isnumber(L, -1))
    luaL_error(L, "sampleformat field must be a number");
  params->sampleFormat = (PaSampleFormat)(lua_tonumber(L, -1));
  lua_pop(L, 1);

  lua_getfield(L, idx, "suggestedlatency");
  if(!lua_isnumber(L, -1))
    luaL_error(L, "suggestedlatency field must be a number");
  params->suggestedLatency = (PaTime)(lua_tonumber(L, -1));
  lua_pop(L, 1);

  params->hostApiSpecificStreamInfo = NULL;
}

static int pa_isformatsupported(lua_State *L)
{
  int narg = lua_gettop(L);
  PaStreamParameters inputparams;
  PaStreamParameters outputparams;
  double samplerate = 0;
  int hasinput = 0;
  int hasoutput = 0;
  PaError err;

  if(narg == 3 
     && lua_istable(L, 1) && lua_istable(L, 2) && lua_isnumber(L, 3))
  {
    hasinput = 1;
    hasoutput = 1;
    pa_readstreamparameters(L, 1, &inputparams);
    pa_readstreamparameters(L, 2, &outputparams);
    samplerate = lua_tonumber(L, 3);
  }
  else if(narg == 3 
     && lua_isnil(L, 1) && lua_istable(L, 2) && lua_isnumber(L, 3))
  {
    hasinput = 0;
    hasoutput = 1;
    pa_readstreamparameters(L, 2, &outputparams);
    samplerate = lua_tonumber(L, 3);
  }
  else if(narg == 3 
     && lua_istable(L, 1) && lua_isnil(L, 2) && lua_isnumber(L, 3))
  {
    hasinput = 1;
    hasoutput = 0;
    pa_readstreamparameters(L, 2, &inputparams);
    samplerate = lua_tonumber(L, 3);
  }
  else
    luaL_error(L, "expected arguments: (table or nil) (table or nil) number");

  err = Pa_IsFormatSupported((hasinput ? &inputparams : NULL), (hasoutput ? &outputparams : NULL), samplerate);

  if(err == paNoError)
  {
    lua_pushboolean(L, 1);
    return 1;
  }
  else
  {
    lua_pushboolean(L, 0);
    lua_pushstring(L, Pa_GetErrorText(err));
    return 2;
  }
  return 0;
}

static int pa_sleep(lua_State *L)
{
  int narg = lua_gettop(L);
  long msec = 0;
  if(narg == 1 && lua_isnumber(L, 1))
    msec = (long)lua_tonumber(L, 1);
  else
    luaL_error(L, "expected arguments: number");
  Pa_Sleep(msec);
  return 0;
}

static const struct luaL_Reg pa_global__ [] = {
  {"version", pa_version},
  {"hostapicount", pa_hostapicount},
  {"hostapicount", pa_hostapicount},
  {"defaulthostapi", pa_defaulthostapi},
  {"hostapiinfo", pa_hostapiinfo},
  {"devicecount", pa_devicecount},
  {"defaultinputdevice", pa_defaultinputdevice},
  {"defaultoutputdevice", pa_defaultoutputdevice},
  {"deviceinfo", pa_deviceinfo},
  {"isformatsupported", pa_isformatsupported},
  {"sleep", pa_sleep},
  {NULL, NULL}
};

DLL_EXPORT int luaopen_libpa(lua_State *L)
{
  lua_newtable(L);
  lua_pushvalue(L, -1);
  lua_setfield(L, LUA_GLOBALSINDEX, "pa");
  luaL_register(L, NULL, pa_global__);

  lua_newtable(L);
  lua_pushnumber(L, paFloat32);
  lua_setfield(L, -2, "float32");
  lua_pushnumber(L, paInt16);
  lua_setfield(L, -2, "int16");
  lua_pushnumber(L, paInt32);
  lua_setfield(L, -2, "int32");
  lua_pushnumber(L, paInt24);
  lua_setfield(L, -2, "int24");
  lua_pushnumber(L, paInt8);
  lua_setfield(L, -2, "int8");
  lua_pushnumber(L, paUInt8);
  lua_setfield(L, -2, "uint8");
  lua_pushnumber(L, paCustomFormat);
  lua_setfield(L, -2, "customformat");
  lua_pushnumber(L, paNonInterleaved);
  lua_setfield(L, -2, "noninterleaved");
  lua_setfield(L, -2, "format");

  torch_ShortTensor_id = luaT_checktypename2id(L, "torch.ShortTensor");
  torch_IntTensor_id = luaT_checktypename2id(L, "torch.IntTensor");
  torch_FloatTensor_id = luaT_checktypename2id(L, "torch.FloatTensor");
  torch_DoubleTensor_id = luaT_checktypename2id(L, "torch.DoubleTensor");

//  pa_id = luaT_newmetatable(L, "sndfile.SndFile", NULL, sndfile_new, sndfile_free, NULL);
//  luaL_register(L, NULL, sndfile_SndFile__);
//  lua_pop(L, 1);
  pa_checkerror(L, Pa_Initialize());

  return 1;
}
