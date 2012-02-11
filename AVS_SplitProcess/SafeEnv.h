#pragma once

// some functions in IScriptEnvironment is not thread-safe, here is a workaround

#include "avisynth.h"

PVideoFrame SafeNewVideoFrame(IScriptEnvironment* env, const VideoInfo& vi, int align = FRAME_ALIGN);

bool SafeMakeWritable(IScriptEnvironment* env, PVideoFrame* pvf);