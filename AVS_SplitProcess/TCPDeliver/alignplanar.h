// Avisynth v2.5.  Copyright 2002 Ben Rudiak-Gould et al.
// http://www.avisynth.org

// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA, or visit
// http://www.gnu.org/copyleft/gpl.html .
//
// Linking Avisynth statically or dynamically with other modules is making a
// combined work based on Avisynth.  Thus, the terms and conditions of the GNU
// General Public License cover the whole combination.
//
// As a special exception, the copyright holders of Avisynth give you
// permission to link Avisynth with independent modules that communicate with
// Avisynth solely through the interfaces defined in avisynth.h, regardless of the license
// terms of these independent modules, and to copy and distribute the
// resulting combined work under terms of your choice, provided that
// every copy of the combined work is accompanied by a complete copy of
// the source code of Avisynth (the version of Avisynth used to produce the
// combined work), being distributed under the terms of the GNU General
// Public License plus this exception.  An independent module is a module
// which is not derived from or based on Avisynth, such as 3rd-party filters,
// import and export plugins, or graphical user interfaces.


#include "../SafeEnv.h"

/******************************
 *******   AlignPlanar   ******
 *****************************/

#ifndef Alignplanar_h
#define Alignplanar_h

AlignPlanar::AlignPlanar(PClip _clip) : GenericVideoFilter(_clip) {}

PVideoFrame __stdcall AlignPlanar::GetFrame(int n, IScriptEnvironment* env) {
    PVideoFrame src = child->GetFrame(n, env);
    const VideoInfo& child_vi = child->GetVideoInfo();
    if (!(child_vi.RowSize(PLANAR_Y_ALIGNED)&(FRAME_ALIGN-1))) return src;
    PVideoFrame dst = SafeNewVideoFrame(env, vi);
    if ((vi.RowSize(PLANAR_Y_ALIGNED)&(FRAME_ALIGN-1))) 
        env->ThrowError("AlignPlanar: [internal error] Returned frame was not aligned!");


    env->BitBlt(dst->GetWritePtr(), dst->GetPitch(), src->GetReadPtr(), src->GetPitch(), child_vi.RowSize(), child_vi.height);
    env->BitBlt(dst->GetWritePtr(PLANAR_V), dst->GetPitch(PLANAR_V), src->GetReadPtr(PLANAR_V), src->GetPitch(PLANAR_V), child_vi.RowSize(PLANAR_V), child_vi.height >> child_vi.GetPlaneHeightSubsampling(PLANAR_V));
    env->BitBlt(dst->GetWritePtr(PLANAR_U), dst->GetPitch(PLANAR_U), src->GetReadPtr(PLANAR_U), src->GetPitch(PLANAR_U), child_vi.RowSize(PLANAR_U), child_vi.height >> child_vi.GetPlaneHeightSubsampling(PLANAR_U));
    return dst;
}


PClip AlignPlanar::Create(PClip clip) 
{
  if (!clip->GetVideoInfo().IsPlanar()) {  // If not planar, already ok.
    return clip;
  }
  else 
    return new AlignPlanar(clip);
}


#endif