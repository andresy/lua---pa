#include <stdio.h>
#include <portaudio.h>
#include <luaT.h>
#include "TH.h"

const void* torch_ShortTensor_id;
const void* torch_IntTensor_id;
const void* torch_FloatTensor_id;
const void* torch_DoubleTensor_id;
const void *pa_stream_id;

typedef struct pa_stream__
{
    PaStream *id;
    int ninchannel;
    int noutchannel;
    PaSampleFormat insampleformat;
    PaSampleFormat outsampleformat;
    lua_State *pa_L;
    const char *callbackerror;
} pa_Stream;

static void pa_checkerror(lua_State *L, PaError err)
{
  if(err != paNoError)
    luaL_error(L, "An error occured while using the portaudio stream: %s", Pa_GetErrorText(err));
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

/* should be a macro with float, short, char, byte (cannot do the 24 bits right, even though i could convert from 32 bit tensor on the fly) */
static int streamcallbackshort(const void *input_, void *output_,
                               unsigned long frameCount,
                               const PaStreamCallbackTimeInfo *timeInfo, PaStreamCallbackFlags statusFlags,
                               void *userData_)
{
  pa_Stream *stream = userData_;
  static THShortStorage outstorage;
  static THShortTensor outtensor;
  static long size[2];
  static long stride[2];
  int ret;

  size[0] = frameCount;
  size[1] = stream->noutchannel;
  stride[0] = 2;
  stride[1] = 1;
  
  outstorage.data = (short int*)output_;
  outstorage.size = frameCount*stream->noutchannel;
  outstorage.refcount = 0;
  outstorage.flag = 0;
  
  outtensor.size = size;
  outtensor.stride = stride;
  outtensor.nDimension = 2;
  outtensor.storage = &outstorage;
  outtensor.storageOffset = 0;
  outtensor.refcount = 0;
  outtensor.flag = 0;

  lua_pushvalue(stream->pa_L, -1);
  luaT_pushudata(stream->pa_L, &outtensor, luaT_checktypename2id(stream->pa_L, "torch.ShortTensor"));
  if(lua_pcall(stream->pa_L, 1, 1, 0))
  {
    stream->callbackerror = lua_tostring(stream->pa_L, -1);
    return paComplete;
  }

  if(lua_toboolean(stream->pa_L, -1))
    ret = paContinue;
  else
    ret = paComplete;
  lua_pop(stream->pa_L, 1);

  return ret;
}

static int pa_writer(lua_State *L, const void* b, size_t size, void* B)
{
  (void)L;
  luaL_addlstring((luaL_Buffer*) B, (const char *)b, size);
  return 0;
}

static int pa_opendefaultstream(lua_State *L)
{
  int numinchan = 0;
  int numoutchan = 0;
  PaSampleFormat sampleformat = 0;
  double samplerate = 0;
  unsigned long nbufframe = 0;
  pa_Stream *stream = NULL;
  int narg = lua_gettop(L);
  int hascallback = 0;
  lua_State *pa_L = NULL;

  if((narg == 5 || (narg == 6 && lua_isfunction(L, 6))) &&
     lua_isnumber(L, 1) && lua_isnumber(L, 2) && lua_isnumber(L, 3) && lua_isnumber(L, 4) && lua_isnumber(L, 5))
  {
    numinchan = (int)lua_tonumber(L, 1);
    numoutchan = (int)lua_tonumber(L, 2);
    sampleformat = (PaSampleFormat)lua_tonumber(L, 3);
    samplerate = (double)lua_tonumber(L, 4);
    nbufframe = (unsigned long)lua_tonumber(L, 5);

    if(narg == 6)
      hascallback = 1;
  }
  else
    luaL_error(L, "expected arguments: number number number number number [function]");

  if(hascallback)
  {
    luaL_Buffer b;
    size_t l;
    const char *str;

    luaL_buffinit(L, &b);
    if(lua_dump(L, pa_writer, &b) != 0)
      luaL_error(L, "invalid callback function -- cannot be dumped");
    luaL_pushresult(&b);
    str = luaL_checklstring(L, -1, &l);

    if(!(pa_L = luaL_newstate()))
      luaL_error(L, "could not allocate new state");

    luaL_openlibs(pa_L);
    if(luaL_loadbuffer(pa_L, str, l, NULL))
      luaL_error(L, "could not load the callback function properly");
    
    if(lua_pcall(pa_L, 0, LUA_MULTRET, 0))
      luaL_error(L, "could not execute the callback function properly: %s", lua_tostring(pa_L, -1));

    if(!lua_isfunction(pa_L, -1))
      luaL_error(L, "the callback function did not return a function");
  }

  stream = luaT_alloc(L, sizeof(pa_Stream));
  stream->id = NULL;
  stream->ninchannel = numinchan;
  stream->noutchannel = numoutchan;
  stream->insampleformat = sampleformat;
  stream->outsampleformat = sampleformat;
  stream->pa_L = pa_L;
  stream->callbackerror = NULL;
  luaT_pushudata(L, stream, pa_stream_id);

  if(hascallback)
    pa_checkerror(L, Pa_OpenDefaultStream(&stream->id, numinchan, numoutchan, sampleformat, samplerate, nbufframe, streamcallbackshort, stream));
  else
    pa_checkerror(L, Pa_OpenDefaultStream(&stream->id, numinchan, numoutchan, sampleformat, samplerate, nbufframe, NULL, NULL));

  return 1;
}

static int pa_stream_close(lua_State *L)
{
  pa_Stream *stream = NULL;
  int narg = lua_gettop(L);
  if(narg == 1 && luaT_isudata(L, 1, pa_stream_id))
    stream = luaT_toudata(L, 1, pa_stream_id);
  else
    luaL_error(L, "expected arguments: Stream");

  if(!stream->id)
    luaL_error(L, "attempt to operate on a closed stream");

  pa_checkerror(L, Pa_CloseStream(stream->id));
  stream->id = NULL;
  return 0;
}

static int pa_stream_free(lua_State *L)
{
  pa_Stream *stream = NULL;
  int narg = lua_gettop(L);
  if(narg == 1 && luaT_isudata(L, 1, pa_stream_id))
    stream = luaT_toudata(L, 1, pa_stream_id);
  else
    luaL_error(L, "expected arguments: Stream");

  if(!stream->id)
    luaL_error(L, "attempt to operate on a closed stream");
  else
    Pa_CloseStream(stream->id);

  /* should also free input/output buffers */
  luaT_free(L, stream);

  return 0;
}

static int pa_stream_start(lua_State *L)
{
  pa_Stream *stream = NULL;
  int narg = lua_gettop(L);
  if(narg == 1 && luaT_isudata(L, 1, pa_stream_id))
    stream = luaT_toudata(L, 1, pa_stream_id);
  else
    luaL_error(L, "expected arguments: Stream");

  if(!stream->id)
    luaL_error(L, "attempt to operate on a closed stream");

  pa_checkerror(L, Pa_StartStream(stream->id));
  return 0;
}

static int pa_stream_abort(lua_State *L)
{
  pa_Stream *stream = NULL;
  int narg = lua_gettop(L);
  if(narg == 1 && luaT_isudata(L, 1, pa_stream_id))
    stream = luaT_toudata(L, 1, pa_stream_id);
  else
    luaL_error(L, "expected arguments: Stream");

  if(!stream->id)
    luaL_error(L, "attempt to operate on a closed stream");

  pa_checkerror(L, Pa_AbortStream(stream->id));
  return 0;
}

static int pa_stream_stop(lua_State *L)
{
  pa_Stream *stream = NULL;
  int narg = lua_gettop(L);
  if(narg == 1 && luaT_isudata(L, 1, pa_stream_id))
    stream = luaT_toudata(L, 1, pa_stream_id);
  else
    luaL_error(L, "expected arguments: Stream");

  if(!stream->id)
    luaL_error(L, "attempt to operate on a closed stream");

  pa_checkerror(L, Pa_StopStream(stream->id));
  return 0;
}

static int pa_stream_isstopped(lua_State *L)
{
  pa_Stream *stream = NULL;
  int narg = lua_gettop(L);
  PaError err = 0;

  if(narg == 1 && luaT_isudata(L, 1, pa_stream_id))
    stream = luaT_toudata(L, 1, pa_stream_id);
  else
    luaL_error(L, "expected arguments: Stream");

  if(!stream->id)
    luaL_error(L, "attempt to operate on a closed stream");

  err = Pa_IsStreamStopped(stream->id);
  if(err == 1)
    lua_pushboolean(L, 1);
  else if(err == 0)
    lua_pushboolean(L, 0);
  else
    pa_checkerror(L, err);

  return 1;
}

static int pa_stream_isactive(lua_State *L)
{
  pa_Stream *stream = NULL;
  int narg = lua_gettop(L);
  PaError err = 0;

  if(narg == 1 && luaT_isudata(L, 1, pa_stream_id))
    stream = luaT_toudata(L, 1, pa_stream_id);
  else
    luaL_error(L, "expected arguments: Stream");

  if(!stream->id)
    luaL_error(L, "attempt to operate on a closed stream");

  err = Pa_IsStreamActive(stream->id);
  if(err == 1)
    lua_pushboolean(L, 1);
  else if(err == 0)
    lua_pushboolean(L, 0);
  else
    pa_checkerror(L, err);

  return 1;
}

static int pa_stream_readavailable(lua_State *L)
{
  pa_Stream *stream = NULL;
  int narg = lua_gettop(L);
  PaError err = 0;

  if(narg == 1 && luaT_isudata(L, 1, pa_stream_id))
    stream = luaT_toudata(L, 1, pa_stream_id);
  else
    luaL_error(L, "expected arguments: Stream");

  if(!stream->id)
    luaL_error(L, "attempt to operate on a closed stream");

  err = Pa_GetStreamReadAvailable(stream->id);
  if(err >= 0)
    lua_pushnumber(L, err);
  else
    pa_checkerror(L, err);

  return 1;
}

static int pa_stream_writeavailable(lua_State *L)
{
  pa_Stream *stream = NULL;
  int narg = lua_gettop(L);
  PaError err = 0;

  if(narg == 1 && luaT_isudata(L, 1, pa_stream_id))
    stream = luaT_toudata(L, 1, pa_stream_id);
  else
    luaL_error(L, "expected arguments: Stream");

  if(!stream->id)
    luaL_error(L, "attempt to operate on a closed stream");

  err = Pa_GetStreamWriteAvailable(stream->id);
  if(err >= 0)
    lua_pushnumber(L, err);
  else
    pa_checkerror(L, err);

  return 1;
}

static int pa_stream_cpuload(lua_State *L)
{
  pa_Stream *stream = NULL;
  int narg = lua_gettop(L);

  if(narg == 1 && luaT_isudata(L, 1, pa_stream_id))
    stream = luaT_toudata(L, 1, pa_stream_id);
  else
    luaL_error(L, "expected arguments: Stream");

  if(!stream->id)
    luaL_error(L, "attempt to operate on a closed stream");

  lua_pushnumber(L, Pa_GetStreamCpuLoad(stream->id));
  return 1;
}

static int pa_stream_writeShort(lua_State *L)
{
  pa_Stream *stream = NULL;
  THShortTensor *data = NULL;
  long nelem = 0;
  PaError err = 0;
  int narg = lua_gettop(L);

  if(narg == 2 && luaT_isudata(L, 1, pa_stream_id) && luaT_isudata(L, 2, torch_ShortTensor_id))
  {
    stream = luaT_toudata(L, 1, pa_stream_id);
    data = luaT_toudata(L, 2, torch_ShortTensor_id);
  }
  else
    luaL_error(L, "expected arguments: Stream ShortTensor");

  if(!stream->id)
    luaL_error(L, "attempt to operate on a closed stream");

  nelem = THShortTensor_nElement(data);
  luaL_argcheck(L, (nelem > 0) && (nelem % stream->noutchannel == 0), 2, "invalid data: number of elements must be > 0 and divisible by the number of channels");
  luaL_argcheck(L, stream->outsampleformat & paInt16, 1, "stream does not support short data");

  data = THShortTensor_newContiguous(data);
  err = Pa_WriteStream(stream->id, THShortTensor_data(data), nelem/stream->noutchannel);
  THShortTensor_free(data);

  if(err == paOutputUnderflowed)
    lua_pushboolean(L, 0);
  else if(err == paNoError)
    lua_pushboolean(L, 1);
  else
    pa_checkerror(L, err);

  return 1;
}

static int pa_stream_callbackerror(lua_State *L)
{
  pa_Stream *stream = NULL;
  int narg = lua_gettop(L);

  if(narg == 1 && luaT_isudata(L, 1, pa_stream_id))
    stream = luaT_toudata(L, 1, pa_stream_id);
  else
    luaL_error(L, "expected arguments: Stream");

  if(!stream->id)
    luaL_error(L, "attempt to operate on a closed stream");

  if(Pa_IsStreamActive(stream->id))
    luaL_error(L, "cannot check callback error on an active stream");

  if(stream->callbackerror)
  {
    lua_pushstring(L, stream->callbackerror);
    return 1;
  }

  return 0;
}

static const struct luaL_Reg pa_stream__ [] = {
  {"close", pa_stream_close},
  {"start", pa_stream_start},
  {"abort", pa_stream_abort},
  {"stop", pa_stream_stop},
  {"isstopped", pa_stream_isstopped},
  {"isactive", pa_stream_isactive},
  {"readavailable", pa_stream_readavailable},
  {"writeavailable", pa_stream_writeavailable},
  {"cpuload", pa_stream_cpuload},
  {"writeShort", pa_stream_writeShort},
  {"callbackerror", pa_stream_callbackerror},
  {NULL, NULL}
};

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
  {"opendefaultstream", pa_opendefaultstream},
  {NULL, NULL}
};

DLL_EXPORT int luaopen_libpa(lua_State *L)
{
  if(sizeof(short) != 2)
    luaL_error(L, "your platform has a strange size for short int type");

  if(sizeof(int) != 4)
    luaL_error(L, "your platform has a strange size for int type");

  if(sizeof(char) != 1)
    luaL_error(L, "your platform has a strange size for char type");

  if(sizeof(float) != 4)
    luaL_error(L, "your platform has a strange size for float type");

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

  pa_stream_id = luaT_newmetatable(L, "pa.Stream", NULL, NULL, pa_stream_free, NULL);
  luaL_register(L, NULL, pa_stream__);
  lua_pop(L, 1);

  pa_checkerror(L, Pa_Initialize());

  return 1;
}
