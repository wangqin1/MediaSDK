// Copyright (c) 2020 Intel Corporation
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#include <algorithm>
#include <iterator>
#include <cmath>

#include "aenc++.h"

#if defined(ENABLE_ADAPTIVE_ENCODE)

mfxStatus AEncInit(mfxHDL* pthis, AEncParam param) {

    if (!pthis) {
        return MFX_ERR_INVALID_VIDEO_PARAM;
    }

    try {
        *pthis = reinterpret_cast<mfxHDL>(new aenc::AEnc());
        aenc::AEnc* a = reinterpret_cast<aenc::AEnc*>(*pthis);
        a->Init(param);
        return MFX_ERR_NONE;
    }
    catch (aenc::Error) {
        return MFX_ERR_INVALID_VIDEO_PARAM;
    }
    catch (...)
    {
        return MFX_ERR_UNKNOWN;
    }
}


void AEncClose(mfxHDL pthis) {
    if (!pthis) {
        return;
    }

    try {
        aenc::AEnc* a = reinterpret_cast<aenc::AEnc*>(pthis);
        a->Close();
        delete a;
        return;
    }
    catch (aenc::Error) {
        return;
    }
    catch (...)
    {
        return;
    }
    
}


mfxStatus AEncProcessFrame(mfxHDL pthis, mfxU32 POC, mfxU8* InFrame, mfxI32 pitch, AEncFrame* OutFrame) {
    if (!pthis) {
        return MFX_ERR_NOT_INITIALIZED;
    }

    try {
        aenc::AEnc* a = reinterpret_cast<aenc::AEnc*>(pthis);
        return a->ProcessFrame(POC, InFrame, pitch, OutFrame);
    }
    catch (aenc::Error) {
        return MFX_ERR_INVALID_VIDEO_PARAM;
    }
    catch (...)
    {
        return MFX_ERR_UNKNOWN;
    }
}

mfxU16 AEncGetIntraDecision(mfxHDL pthis, mfxU32 displayOrder) {
    if (!pthis) {
        return 0;
    }

    try {
        aenc::AEnc* a = reinterpret_cast<aenc::AEnc*>(pthis);
        return a->GetIntraDecision(displayOrder);
    }
    catch (aenc::Error) {
        return 0;
    }
    catch (...)
    {
        return 0;
    }
}

mfxU16 AEncGetPersistenceMap(mfxHDL pthis, mfxU32 displayOrder, mfxU16 PMap[16 * 8]) {
    if (!pthis) {
        return 0;
    }

    try {
        aenc::AEnc* a = reinterpret_cast<aenc::AEnc*>(pthis);
        return a->GetPersistenceMap(displayOrder, PMap);
    }
    catch (aenc::Error) {
        return 0;
    }
    catch (...)
    {
        return 0;
    }
}
void AEncUpdatePFrameBits(mfxHDL pthis, mfxU32 displayOrder, mfxU32 bits, mfxU32 QpY, mfxU32 ClassCmplx) {
    if (!pthis) {
        return;
    }

    try {
        aenc::AEnc* a = reinterpret_cast<aenc::AEnc*>(pthis);
        return a->UpdatePFrameBits(displayOrder, bits, QpY, ClassCmplx);
    }
    catch (aenc::Error) {
        return;
    }
    catch (...)
    {
        return;
    }
}


namespace aenc {
    const uint32_t APQ_Lookup[10][10][3] = {
        // 0          1          2          3          4          5          6          7          8          9
        {{ 0,3,0 }, { 0,3,0 }, { 0,3,0 }, { 0,0,0 }, { 0,0,0 }, { 0,0,0 }, { 0,0,0 }, { 0,0, 0}, { 0,0,0 }, { 0,0,0 }}, //0
        {{ 0,3,0 }, { 0,3,0 }, { 0,3,0 }, { 0,0,0 }, { 0,0,0 }, { 0,0,0 }, { 0,0,0 }, { 0,0,0 }, { 0,0,0 }, { 0,0,0 }}, //1
        {{ 0,3,0 }, { 0,3,0 }, { 0,3,0 }, { 0,0,0 }, { 0,0,0 }, { 0,0,0 }, { 0,0,0 }, { 0,0,0 }, { 0,0,0 }, { 0,0,0 }}, //2
        {{ 0,3,0 }, { 0,3,0 }, { 0,3,0 }, { 2,0,0 }, { 1,2,0 }, { 2,0,0 }, { 2,0,0 }, { 2,0,0 }, { 2,0,0 }, { 2,0,0 }}, //3
        {{ 0,3,0 }, { 0,3,0 }, { 0,3,0 }, { 2,0,0 }, { 1,2,0 }, { 1,2,0 }, { 1,2,0 }, { 2,0,0 }, { 2,0,0 }, { 2,0,0 }}, //4
        {{ 1,2,0 }, { 1,2,0 }, { 1,2,0 }, { 1,2,0 }, { 1,2,0 }, { 1,2,0 }, { 1,2,0 }, { 1,2,0 }, { 1,2,0 }, { 1,2,0 }}, //5
        {{ 1,2,0 }, { 1,2,0 }, { 1,2,0 }, { 1,2,0 }, { 1,2,0 }, { 1,2,0 }, { 1,2,0 }, { 1,2,0 }, { 1,2,0 }, { 1,1,2 }}, //6
        {{ 1,2,0 }, { 1,2,0 }, { 1,2,0 }, { 1,2,0 }, { 1,2,0 }, { 1,2,0 }, { 1,2,0 }, { 1,2,0 }, { 1,2,0 }, { 1,1,2 }}, //7
        {{ 1,2,0 }, { 1,2,0 }, { 1,2,0 }, { 1,2,0 }, { 1,2,0 }, { 1,2,0 }, { 1,2,0 }, { 1,2,0 }, { 1,2,0 }, { 1,1,2 }}, //8
        {{ 1,2,0 }, { 1,2,0 }, { 1,2,0 }, { 1,2,0 }, { 1,2,0 }, { 1,2,0 }, { 1,2,0 }, { 1,2,0 }, { 1,2,0 }, { 1,1,2 }}  //9
    };

    InternalFrame::operator ExternalFrame() {
        ExternalFrame ext;
        ext.POC = POC;
        ext.SceneChanged = SceneChanged;
        ext.RepeatedFrame = RepeatedFrame;
        ext.TemporalComplexity = TemporalComplexity;
        ext.LTR = LTR;
        ext.MiniGopSize = MiniGopSize;
        ext.PyramidLayer = PyramidLayer;
        ext.Type = Type;
        ext.DeltaQP = DeltaQP;
        ext.ClassAPQ = ClassAPQ;
        ext.ClassCmplx = ClassSCTSC;
        ext.KeepInDPB = KeepInDPB;
        ext.RemoveFromDPB = RemoveFromDPB;
        ext.RefList = RefList;
        for (uint16_t i = 0; i < 16 * 8; i++)
            ext.PMap[i] = PMap[i];
        return ext;
    }


    ExternalFrame::operator AEncFrame() {
        AEncFrame out;
        out.POC = POC;
        out.SceneChanged = SceneChanged;
        out.RepeatedFrame = RepeatedFrame;
        out.TemporalComplexity = TemporalComplexity;
        out.LTR = LTR;
        out.MiniGopSize = MiniGopSize;
        out.PyramidLayer = PyramidLayer;
        out.DeltaQP = DeltaQP;
        out.ClassAPQ = ClassAPQ;
        out.ClassCmplx = ClassCmplx;
        out.KeepInDPB = KeepInDPB;

        switch (Type) {
        case FrameType::IDR:
            out.Type = MFX_FRAMETYPE_IDR;
            break;
        case FrameType::I:
            out.Type = MFX_FRAMETYPE_I;
            break;
        case FrameType::P:
            out.Type = MFX_FRAMETYPE_P;
            break;
        case FrameType::B:
            out.Type = MFX_FRAMETYPE_B;
            break;
        default:
            throw Error("wrong frame type in ExternalFrame::operator AEncFrame()");

        }

        if (RemoveFromDPB.size() > sizeof(out.RemoveFromDPB) / sizeof(out.RemoveFromDPB[0])) {
            throw Error("wrong RemoveFromDPB size");
        }
        out.RemoveFromDPBSize = static_cast<mfxU32>(RemoveFromDPB.size());
        std::copy(RemoveFromDPB.begin(), RemoveFromDPB.end(), out.RemoveFromDPB);

        if (RefList.size() > sizeof(out.RefList) / sizeof(out.RefList[0])) {
            throw Error("wrong RefList size");
        }
        out.RefListSize = static_cast<mfxU32>(RefList.size());
        std::copy(RefList.begin(), RefList.end(), out.RefList);

        for (mfxU16 i = 0; i < 16 * 8; i++)
            out.PMap[i] = PMap[i];
        return out;
    }


    void ExternalFrame::print() {
        //if (SceneChanged) {
            printf(" frame[%4d] %s %s %s %s TC %d gop %d pyr %d qp %3d %s remove %4d:%d ref %4d:%d\n",
                POC,
                Type == FrameType::IDR ? "IDR" :
                Type == FrameType::I ? "I  " :
                Type == FrameType::P ? " P " :
                Type == FrameType::B ? "  B" :
                "UND",
                SceneChanged ? "SCD" : "   ",
                RepeatedFrame ? "R" : " ",
                LTR          ? "LTR" : "   ",
                TemporalComplexity,
                MiniGopSize,
                PyramidLayer, DeltaQP,
                KeepInDPB ? "keep":"    ",
                RemoveFromDPB.size() < 1 ? -1 : RemoveFromDPB[0],
                RemoveFromDPB.size() < 2 ? -1 : RemoveFromDPB[1],
                RefList.size() < 1 ? -1 : RefList[0],
                RefList.size() < 2 ? -1 : RefList[1]);
        //}
    }


    void AEnc::Init(AEncParam param) {
        //check param

        if (param.MaxMiniGopSize != 1 && param.MaxMiniGopSize != 2 && param.MaxMiniGopSize != 4 && param.MaxMiniGopSize != 8) {
            throw Error("wrong MaxMiniGopSize at Init");
        }

        if (param.MinGopSize >= param.MaxGopSize || param.MaxGopSize > param.MaxIDRDist || param.MaxIDRDist % param.MaxGopSize != 0) {
            //if this condition is changed, then MakeIFrameDecision() should be updated accordingly
            throw Error("wrong GOP size at Init");
        }

        if (param.MinGopSize > param.MaxGopSize - param.MaxMiniGopSize) {
            throw Error("wrong adaptive GOP parameters at Init");
        }

        if (param.ColorFormat != MFX_FOURCC_NV12 && param.ColorFormat != MFX_FOURCC_RGB4) {
            throw Error("wrong color format at Init");
        }

        //save params
        InitParam = param;

        //init SCD
        mfxStatus sts = Scd.Init(param.FrameWidth, param.FrameHeight, param.Pitch, 0/*progressive*/, param.ColorFormat == MFX_FOURCC_NV12);
        if (sts != MFX_ERR_NONE) {
            throw Error("SCD initialization failed");
        }
        sts = LtrScd.Init(param.FrameWidth, param.FrameHeight, param.Pitch, 0/*progressive*/, param.ColorFormat == MFX_FOURCC_NV12);
        if (sts != MFX_ERR_NONE) {
            throw Error("LtrSCD initialization failed");
        }
        LastPFrameNoisy = 0;
        LastPFrameQp = 0;
        m_isLtrOn = InitParam.ALTR ? true : false;
        for (mfxU16 i = 0; i < 16 * 8; i++)
            PersistenceMap[i] = 0;
    }


    void AEnc::Close(){
        Scd.Close();
        LtrScd.Close();
    }


    mfxStatus AEnc::ProcessFrame(uint32_t POC, const uint8_t* InFrame, int32_t pitch, AEncFrame* OutFrame) {
        InternalFrame f;
        f.POC = POC;
        f.PPyramidIdx = 0;

        if (InFrame != nullptr) {
            //run SCD, generates stat, saves GOP size and SC
            RunScd(InFrame, pitch, f);

            //save per frame stat
            SaveStat(f);

            MakeIFrameDecision(f);

            //save frame to buffer
            FrameBuffer.push_back(f);
        }
        else {
            //EOS
            f.Type = FrameType::DUMMY;
            while (FrameBuffer.size() < InitParam.MaxMiniGopSize) {
                FrameBuffer.push_back(f);
            }
        }


        //make mini GOP decision
        uint32_t MiniGopSize;
        if (MakeMiniGopDecision(MiniGopSize)) {
            //GOP decided, compute ref list and QP for each frame in mini-GOP
            for (uint32_t i = 0; i < MiniGopSize; i++) {
                f = FrameBuffer.front();
                FrameBuffer.pop_front();

                MarkFrameInMiniGOP(f, MiniGopSize, i);

                ComputeStat(f);

                MakeAltrArefDecision(f);

                BuildRefList(f);
                AdjustQp(f);
                MakeDbpDecision(f);
                SaveFrameTypeInfo(f);

                OutputBuffer.push_back(f);
            }
        }

        //output frame
        return OutputDecision(OutFrame);
    }


    void AEnc::RunScd(const uint8_t* InFrame, int32_t pitch, InternalFrame &f) {
        Scd.PutFrameProgressive(const_cast<uint8_t*>(InFrame), pitch, false/*LTR*/);

        f.MiniGopSize = Scd.GetFrameGopSize();
        if (InitParam.CodecId == MFX_CODEC_HEVC && InitParam.MaxMiniGopSize == 2) {
            int32_t  sc = Scd.Get_frame_SC();
            int32_t qsc = (sc < 2048) ? (sc >> 9) : (4 + ((sc - 2048) >> 10));
            qsc = (qsc < 0) ? 0 : ((qsc > 9) ? 9 : qsc);

            int32_t  mv = Scd.Get_frame_MVSize();
            int32_t qmv = 0;
            if (mv < 1024) {
                qmv = (mv < 256) ? 0 : ((mv < 512) ? 1 : 2);
            }
            else {
                qmv = 3 + ((mv - 1024) >> 10);
                qmv = (qmv < 0) ? 0 : ((qmv > 9) ? 9 : qmv);
            }

            int32_t  MV_TH[10] = { 2,4,4,4,4,4,4,4,4,6 };
            uint32_t miniGopSize = (qmv < MV_TH[qsc]) ? 2 : 1;
            f.MiniGopSize = miniGopSize;
        }

        f.SceneChanged = Scd.Get_frame_shot_Decision();
        f.RepeatedFrame = Scd.Get_Repeated_Frame_Flag();
        f.TemporalComplexity = Scd.Get_frame_Temporal_complexity();
        f.UseLtrAsReference = true;

        Scd.GetImageAndStat(f.ScdImage, f.ScdStat, ASCReference_Frame, ASCprevious_frame_data);
        Scd.get_PersistenceMap(f.PMap, false);
        Scd.get_PersistenceMap(PersistenceMap, true);
    }


    void AEnc::SaveStat(InternalFrame& f){
        SaveAltrStat(f);
        SaveArefStat(f);
        SaveApqStat(f);
    }


   void AEnc::SaveAltrStat(InternalFrame& f) {
        f.MV = (int32_t)Scd.Get_frame_MV0();
        f.highMvCount = Scd.Get_frame_RecentHighMvCount();
        ASC_LTR_DEC scd_LTR_hint = NO_LTR;
        Scd.get_LTR_op_hint(scd_LTR_hint);
        f.LtrOnHint = (scd_LTR_hint == NO_LTR) ? 0 : 1;
    }


    void AEnc::SaveArefStat(InternalFrame& f) {
        f.MV = (int32_t)Scd.Get_frame_MV0();
        f.CORR = Scd.Get_frame_mcTCorr();
    }


    void AEnc::SaveApqStat(InternalFrame& f) {
        f.SC = Scd.Get_frame_Spatial_complexity();
        f.TSC = Scd.Get_frame_Temporal_complexity();
        f.MVSize = Scd.Get_frame_MVSize();
        f.Contrast = Scd.Get_frame_Contrast();
    }


    void AEnc::MakeIFrameDecision(InternalFrame& f) {
        //function should be called strictly before MakeMiniGopDecision()
        //first frame in sequence
        if (f.POC == 0) {
            MarkFrameAsIDR(f);
            return;
        }

        //strict I decision
        if (InitParam.StrictIFrame) {
            if (f.POC % InitParam.GopPicSize == 0) {
                if (f.POC % InitParam.MaxIDRDist == 0) {
                    MarkFrameAsIDR(f);
                }
                else {
                    MarkFrameAsI(f);
                }
            }
            return;
        }

        //protected interval between I frames.
        const uint32_t CurrentGOPSize = f.POC - PocOfLastIFrame;
        if (CurrentGOPSize < InitParam.MinGopSize) {
            //GOP is too small, keep current frame as P, even if it is scene change
            return;
        }

        //insert IDR if max IDR interval reached
        uint32_t CurrentIdrInterval = f.POC - PocOfLastIdrFrame;
        if (CurrentIdrInterval >= InitParam.MaxIDRDist) {
            MarkFrameAsIDR(f);
            return;
        }

        // For AVC use IDR, for HEVC use CRA
        if (f.SceneChanged && InitParam.CodecId == MFX_CODEC_AVC) {
            MarkFrameAsIDR(f);
            return;
        }

        //scene changed or max GOP size reached, insert I
        if (f.SceneChanged || CurrentGOPSize >= InitParam.MaxGopSize) {
            MarkFrameAsI(f);
        }
    }


    bool AEnc::MakeMiniGopDecision(uint32_t& GOPDecision) {
        if (FrameBuffer.size() < InitParam.MaxMiniGopSize) {
            return false;
        }

        //EOS
        if (FrameBuffer[0].Type == FrameType::DUMMY) {
            return false;
        }

        GOPDecision = std::min(GetMiniGopSizeCommon(), GetMiniGopSizeAGOP());
        return true;
    }


    uint32_t AEnc::GetMiniGopSizeCommon() {
        //find start of next mini GOP
        uint32_t n = 1;
        for (InternalFrame f : FrameBuffer) {
            //check if we need additional P frame
            //first frame can't be dummy, scene change frame is usually I, except cases when it is too close to previous I
            if (f.Type == FrameType::IDR || f.Type == FrameType::DUMMY || (!InitParam.StrictIFrame && f.SceneChanged)) {
                n = (n == 1 ? 1 : n - 1);
                break;
            }

            //no additional P, just close mini GOP
            if (f.Type == FrameType::I) {
                break;
            }
            n++;
        }
        //n is mini GOP size from B to P/I inclusive
        return n;
    }


    uint32_t AEnc::GetMiniGopSizeAGOP() {
        //function tries to assemble as long mini GOP as possible, ignoring
        //occasional frames with smaller by one GOP size
        if (!InitParam.AGOP) {
            return InitParam.MaxMiniGopSize;
        }

        for (uint32_t CurMiniGOPSize = InitParam.MaxMiniGopSize; CurMiniGOPSize > 1; CurMiniGOPSize /= 2) {
            uint32_t FullSizeCount = 0, HalfSizeCount = 0, FrameCount = 0;
            for (; FrameCount < CurMiniGOPSize; FrameCount++) {
                if (FrameBuffer[FrameCount].MiniGopSize >= CurMiniGOPSize) {
                    FullSizeCount++;
                }
                if (FrameBuffer[FrameCount].MiniGopSize == CurMiniGOPSize / 2) {
                    HalfSizeCount++;
                }
                if (FrameBuffer[FrameCount].MiniGopSize <= CurMiniGOPSize / 4) {
                    break;
                }
            }

            if (FrameCount <= CurMiniGOPSize / 2) {
                //not enough frames for current mini GOP size, try smaller one
                continue;
            }
            if (FullSizeCount <= HalfSizeCount) {
                //not enough frames with long mini GOP size, try smaller GOP
                continue;
            }

            return FrameCount;
        }

        return 1; //we tried everything else

    }

    void AEnc::SaveFrameTypeInfo(InternalFrame& f) {
        if (FrameBuffer.size())
        {
            FrameBuffer.front().PrevType = f.Type;
            FrameBuffer.front().PPyramidLayer = f.PPyramidLayer;
            FrameBuffer.front().PPyramidIdx = f.PPyramidIdx;
            if (f.PrevType == FrameType::P && f.Type == FrameType::B && OutputBuffer.size())
                OutputBuffer.back().DeltaQP = 0;
        }
    }

    void AEnc::ComputeStat(InternalFrame& f) {
        if (InitParam.ALTR) ComputeStatAltr(f);
        if (InitParam.AREF) ComputeStatAref(f);
        if (InitParam.APQ) ComputeStatApq(f);
    }


    void AEnc::MakeDbpDecision(InternalFrame& f) {
        if (InitParam.ALTR) MakeDbpDecisionLtr(f);
        if (InitParam.AREF) MakeDbpDecisionAref(f);
    }

    void AEnc::BuildRefList(InternalFrame& f) {
        if (InitParam.ALTR) BuildRefListLtr(f);
        if (InitParam.AREF) BuildRefListAref(f);
    }


    void AEnc::AdjustQp(InternalFrame& f) {
        f.DeltaQP = 0;
        if (InitParam.ALTR) AdjustQpLtr(f);
        if (InitParam.AREF) AdjustQpAref(f);
        if (InitParam.APQ) AdjustQpApq(f);
        if (InitParam.AGOP) {
            if (!InitParam.ALTR && !InitParam.AREF && !InitParam.APQ)
                AdjustQpAgop(f);
        }
    }

    void AEnc::MakeAltrArefDecision(InternalFrame& f) {
        if (InitParam.ALTR) MakeAltrDecision(f);
        if (InitParam.AREF) MakeArefDecision(f);
    }


    void AEnc::ComputeStatAltr(InternalFrame& f) {
        if (f.Type == FrameType::I || f.Type == FrameType::IDR || f.LTR) {
            return;
        }


        mfxI32 iMV = f.MV;
        if (iMV > 4000) iMV = 4000;
        mfxI32 prevAvgMV0 = m_avgMV0;
        if (m_avgMV0 > 8) {
            m_avgMV0 = prevAvgMV0 + (iMV - prevAvgMV0) / 4;
        }


        mfxU32 sceneTransition = 0;
        //check if current frame should use LTR as reference
        if(!f.LTR){
            LtrScd.SetImageAndStat(f.ScdImage, f.ScdStat, ASCCurrent_Frame, ASCcurrent_frame_data);
            LtrScd.RunFrame_LTR();

            mfxU32 sctrFlag = LtrScd.Get_frame_LTR_Decision();
            if (f.POC <= 16)
                sctrFlag = 0;
            m_sceneTranBuffer[f.POC % 8] = sctrFlag;
            mfxI32 cnt = 0;
            for (mfxI32 i = 0; i < 8; i++) {
                if (m_sceneTranBuffer[i])
                    cnt++;
            }
            sceneTransition = (cnt == 8) ? 1 : 0;
        }

        f.SceneTransition = sceneTransition ? true : false;
        if (f.SceneTransition) {
            m_isLtrOn = false;
            f.UseLtrAsReference = false; //TODO: this will be overwritten by MakeAltrDecision
        }
    }



    void AEnc::ComputeStatAref(InternalFrame& f) {
        if (f.Type != FrameType::I && f.Type != FrameType::IDR)
        {
            m_hasLowActivity = 0;
            m_prevTemporalActs[f.MiniGopIdx] = (f.MV > 1000) ? 1 : 0;
            mfxI32 cnt = 0;
            for (mfxI32 i = 0; i < 8; i++) {
                if (m_prevTemporalActs[i])
                    cnt++;
            }
            m_hasLowActivity = (cnt < 3) ? 1 : 0;
        }
        else
        {
            m_hasLowActivity = 0;
            for (mfxI32 i = 0; i < 8; i++)
                m_prevTemporalActs[i] = 0;
        }
    }

    const mfxF64 DefModel[10][10][3] = {
        // 0                    1                     2                     3                    4          5          6          7          8          9
        {{ 0,     0,     0 }, { 0,      0,     0 }, { 0,      0,     0 }, { 0,     0,     0 }, { 0,0,0 }, { 0,0,0 }, { 0,0,0 }, { 0,0,0 }, { 0,0,0 }, { 0,0,0 }}, //0
        {{ 0,     0,     0 }, { 0,      0,     0 }, { 0,      0,     0 }, { 0,     0,     0 }, { 0,0,0 }, { 0,0,0 }, { 0,0,0 }, { 0,0,0 }, { 0,0,0 }, { 0,0,0 }}, //1
        {{ 0,     0,     0 }, { 0.2167, 0.4914,1 }, { 0,      0,     0 }, { 0,     0,     0 }, { 0,0,0 }, { 0,0,0 }, { 0,0,0 }, { 0,0,0 }, { 0,0,0 }, { 0,0,0 }}, //2
        {{ 0.0830,0.6201,1 }, { 0.0916, 0.7462,1 }, { 0.3533, 0.5491,1 }, { 0,     0,     0 }, { 0,0,0 }, { 0,0,0 }, { 0,0,0 }, { 0,0,0 }, { 0,0,0 }, { 0,0,0 }}, //3
        {{ 0.1455,0.4302,1 }, { 0.0580, 0.7937,1 }, { 0.4327, 0.4359,1 }, { 0.2197,0.7141,1 }, { 0,0,0 }, { 0,0,0 }, { 0,0,0 }, { 0,0,0 }, { 0,0,0 }, { 0,0,0 }}, //4
        {{ 0,     0,     0 }, { 0.1136, 0.7446,1 }, { 0.1770, 0.6730,1 }, { 0.0139,1.4547,0 }, { 0,0,0 }, { 0,0,0 }, { 0,0,0 }, { 0,0,0 }, { 0,0,0 }, { 0,0,0 }}, //5
        {{ 0.0617,0.8463,0 }, { 0.0454, 0.9545,0 }, { 0.4038, 0.4899,1 }, { 0.2234,0.7087,1 }, { 0,0,0 }, { 0,0,0 }, { 0,0,0 }, { 0,0,0 }, { 0,0,0 }, { 0,0,0 }}, //6
        {{ 0,0,0 }, { 0,0,0 }, { 0,0,0 }, { 0,0,0 }, { 0,0,0 }, { 0,0,0 }, { 0,0,0 }, { 0,0,0 }, { 0,0,0 }, { 0,0,0 }}, //7
        {{ 0,0,0 }, { 0,0,0 }, { 0,0,0 }, { 0,0,0 }, { 0,0,0 }, { 0,0,0 }, { 0,0,0 }, { 0,0,0 }, { 0,0,0 }, { 0,0,0 }}, //8
        {{ 0,0,0 }, { 0,0,0 }, { 0,0,0 }, { 0,0,0 }, { 0,0,0 }, { 0,0,0 }, { 0,0,0 }, { 0,0,0 }, { 0,0,0 }, { 0,0,0 }}  //9
    };

    void AEnc::UpdatePFrameBits(uint32_t frm, uint32_t size, uint32_t qp, uint32_t ClassCmplx)
    {
        if (!InitParam.APQ || InitParam.GopPicSize<8) return;
        mfxU32 width = InitParam.SrcFrameWidth;
        mfxU32 height = InitParam.SrcFrameHeight;
        mfxU32 Trellis = 0;
        mfxU32 lastQp  = LastPFrameQp;
        mfxU32 SC = (ClassCmplx >> 6) & 0xf;
        mfxU32 TSC = (ClassCmplx >> 2) & 0xf;
        mfxU32 MVQ = (ClassCmplx) & 0x3;
        mfxF64 BRC_CONST_MUL_P8 = DefModel[SC][TSC][0];
        mfxF64 BRC_CONST_EXP_R_P8 = DefModel[SC][TSC][1];
        frm;

        mfxF64 BitsDesiredFrame = (mfxF64)(width * height * 12) / (pow(pow(2.0, ((mfxF64)qp - 12.0) / 6.0) / BRC_CONST_MUL_P8, 1.0 / BRC_CONST_EXP_R_P8));
        mfxF64 rat = (mfxF64)(size) / BitsDesiredFrame;
        mfxU32 adjust;
        if (!DefModel[SC][TSC][2] || MVQ > 1 || !lastQp || lastQp > qp + 1) {
            adjust = 0;
        }
        else {
            adjust = (Trellis ? (rat > 0.85 ? 1 : 0) : (rat > 1.15 ? 1 : 0));
        }
        //printf("FRM %d W %d H %d SC %d TC %d MVQ %d PQP qpY %d Rate %d mul %lf exp %lf Est %lf Rat %lf Adj %d\n",
        //    frm, width, height, SC, TSC, MVQ, qp, size, BRC_CONST_MUL_P8, BRC_CONST_EXP_R_P8, BitsDesiredFrame, rat, adjust);
        LastPFrameNoisy = adjust;
        LastPFrameQp = qp;
    }

    std::deque <InternalFrame > ::iterator AEnc::FindInternalFrame(mfxU32 displayOrder)
    {
        struct CompareByDisplayOrder
        {
            mfxU32 m_DispOrder;
            CompareByDisplayOrder(mfxU32 frameOrder) : m_DispOrder(frameOrder) {}
            bool operator ()(InternalFrame const & frame) const { return frame.POC == m_DispOrder; }
        };        
        auto outframe = std::find_if(FrameBuffer.begin(), FrameBuffer.end(), CompareByDisplayOrder(displayOrder));
        return outframe;
    }

    std::deque <ExternalFrame > ::iterator AEnc::FindExternalFrame(mfxU32 displayOrder)
    {
        struct CompareByDisplayOrder
        {
            mfxU32 m_DispOrder;
            CompareByDisplayOrder(mfxU32 frameOrder) : m_DispOrder(frameOrder) {}
            bool operator ()(ExternalFrame const & frame) const { return frame.POC == m_DispOrder; }
        };
        auto outframe = std::find_if(OutputBuffer.begin(), OutputBuffer.end(), CompareByDisplayOrder(displayOrder));
        return outframe;
    }

    mfxU16 AEnc::GetIntraDecision(mfxU32 displayOrder)
    {
        FrameType ftype = FrameType::UNDEF;
        auto internalMatchFrame = FindInternalFrame(displayOrder);
        if (internalMatchFrame != FrameBuffer.end()) 
        {
            ftype = internalMatchFrame->Type;
        }
        else 
        {
            auto externalMatchFrame = FindExternalFrame(displayOrder);
            if (externalMatchFrame != OutputBuffer.end())
            {
                ftype = externalMatchFrame->Type;
            }
        }
        switch (ftype) {
        case FrameType::IDR:
            return MFX_FRAMETYPE_IDR;
            break;
        case FrameType::I:
            return MFX_FRAMETYPE_I;
            break;
        default:
            return 0;
        }
        return 0;
    }

    mfxU16 AEnc::GetPersistenceMap(mfxU32 , mfxU16 PMap[16 * 8])
    {
        mfxU16 count = 0;
        for (mfxU32 i = 0; i < 16 * 8; i++) {
            PMap[i] = PersistenceMap[i];
            if (PMap[i]) count++;
        }
        return count;
    }

    void AEnc::ComputeStatApq(InternalFrame& f) {
        uint32_t SC, TSC, MVSize, MVQ, Contrast;
        SC = f.SC;
        TSC = f.TSC;
        MVSize = f.MVSize;
        MVQ = (MVSize < 640) ? 0 : ((MVSize < 2048) ? 1 : 2);
        Contrast = f.Contrast;
        // Lookup APQ Class
        f.ClassAPQ = APQ_Lookup[SC][TSC][MVQ];
        f.ClassSCTSC = (SC << 6) + (TSC << 2) + MVQ;
        // Differentiate Low complexity using contrast
        if (Contrast > 89 && SC > 0 && SC < 5) {
            // Adjust Class (3->0, 0->2/0->1, 2->1)
            if (f.ClassAPQ == 3)
                f.ClassAPQ = 0;
            else if (f.ClassAPQ == 0) {
                if (MVQ) f.ClassAPQ = 2;
                else     f.ClassAPQ = 1;
            }
            else if (f.ClassAPQ == 2)
                f.ClassAPQ = 1;
        }
        if (f.SceneChanged) {
            LastPFrameNoisy = 0;
            LastPFrameQp    = 0;
        }
        if (LastPFrameNoisy) {
            // APQ Correction
            if (f.ClassAPQ == 1)      f.ClassAPQ = 2;
            else if (f.ClassAPQ == 2) f.ClassAPQ = 0;
            else if (f.ClassAPQ == 0) f.ClassAPQ = 3;
        }
        //printf("FRM %d SC %d TSC %d MVSize %d MVQ %d Contrast %d SCTSC %d APQ %d\n", f.POC, SC, TSC, MVSize, MVQ, Contrast, f.ClassSCTSC, f.ClassAPQ);
    }


    void AEnc::MakeDbpDecisionLtr(InternalFrame& f) {
        //if current frame is LTR remove old LTR from DPB
        if (f.LTR) {
            RemoveFrameFromDPB(f, [=](InternalFrame& x) {return x.LTR; });

            //add new LTR to DPB
            f.KeepInDPB = true;
            DpbBuffer.push_back(f);
        }
    }


    void AEnc::MakeDbpDecisionAref(InternalFrame& f) {
        if (f.LTR) {
            RemoveFrameFromDPB(f, [=](InternalFrame& x) {return x.LTR; });
            f.KeepInDPB = true;
            DpbBuffer.push_back(f);
        }
    }


    void AEnc::BuildRefListLtr(InternalFrame& f) {
        if (f.Type == FrameType::P && f.UseLtrAsReference)
        {
            auto LtrFrame = std::find_if(DpbBuffer.begin(), DpbBuffer.end(), [](InternalFrame& x) {return x.LTR; });
            if (LtrFrame != DpbBuffer.end()) {
                f.RefList.push_back(LtrFrame->POC);
            }
        }
    }

    void AEnc::BuildRefListAref(InternalFrame& f) {
        if (f.Type == FrameType::P)
        {
            auto KeyPFrame = std::find_if(DpbBuffer.begin(), DpbBuffer.end(), [](InternalFrame& x) {return x.LTR; });
            if (KeyPFrame != DpbBuffer.end()) {
                f.RefList.push_back(KeyPFrame->POC);
            }
        }
    }


    void AEnc::AdjustQpLtr(InternalFrame& f) {
        if (f.LTR) {
            //if current frame is LTR boost QP
            if (f.POC == 0) {
                DeltaQpForLtrFrame = -4;
            }
            else {
                DeltaQpForLtrFrame = (m_avgMV0 > 1500 || (f.POC - LtrPoc) < 32) ? (-2) : (-4);
            }

            f.DeltaQP = DeltaQpForLtrFrame;
        }
        else {
            if (!InitParam.APQ) {
                //use default QP offset for non-LTR frame
                if ((f.MiniGopType == 4 || f.MiniGopType == 8) && f.PyramidLayer) {
                    f.DeltaQP = f.PyramidLayer;
                }
            }
        }
    }


    void AEnc::AdjustQpAref(InternalFrame& f) {
        if (f.Type == FrameType::P) {
            if (f.LTR) {
                f.DeltaQP = (f.SC > 4 && m_hasLowActivity) ? (-4) : (-2);
            }
            else
            {
                f.DeltaQP = 0;
            }
        }
        else if (f.Type == FrameType::B) {  // only CQP will use this DeltaQP
            if (!InitParam.APQ) {
                //use default QP offset for non-LTR frame
                if ((f.MiniGopType == 4 || f.MiniGopType == 8) && f.PyramidLayer) {
                    f.DeltaQP = f.PyramidLayer;
                }
            }
        }
    }


    void AEnc::AdjustQpApq(InternalFrame& f) {
        if (f.Type == FrameType::I || f.Type == FrameType::IDR || f.Type == FrameType::P) {
            return;
        }
        uint32_t GOPSize = f.MiniGopType;
        if (GOPSize == 8)
        {
            uint32_t level = std::max(1u, std::min(3u, f.PyramidLayer));
            uint32_t clsAPQ = std::max(0, std::min(3, f.ClassAPQ));
            f.DeltaQP = 1;

            if (clsAPQ == 1) {
                switch (level) {
                case 3:
                    f.DeltaQP += 2;
                case 2:
                    f.DeltaQP += 1;
                case 1:
                default:
                    f.DeltaQP += 2;
                    break;
                }
            }
            else if (clsAPQ == 2) {
                switch (level) {
                case 3:
                    f.DeltaQP += 2;
                case 2:
                    f.DeltaQP += 1;
                case 1:
                default:
                    f.DeltaQP += 1;
                    break;
                }
            }
            else if (clsAPQ == 3) {
                switch (level) {
                case 3:
                    f.DeltaQP += 1;
                case 2:
                    f.DeltaQP += 1;
                case 1:
                default:
                    f.DeltaQP -= 1;
                    break;
                }
            }
            else  {
                switch (level) {
                case 3:
                    f.DeltaQP += 2;
                case 2:
                    f.DeltaQP += 1;
                case 1:
                default:
                    f.DeltaQP += 0;
                    break;
                }
            }
        }
        else if (GOPSize == 4)
        {
            f.DeltaQP = 1 + f.PyramidLayer; // Default
        }
        else // if (GOPSize == 2)
        {
            f.DeltaQP = 3;              // Default
        }
    }

    void AEnc::AdjustQpAgop(InternalFrame& f)
    {
        uint32_t GOPSize = f.MiniGopType;
        if (f.Type == FrameType::I || f.Type == FrameType::IDR || (f.Type == FrameType::P && GOPSize > 4)) {
            return;
        }
        if (f.PyramidLayer)
        {
            if (GOPSize == 8)
            {
                f.DeltaQP = f.PyramidLayer + 1;
            }
            else if (GOPSize == 4)
            {
                f.DeltaQP = f.PyramidLayer + 1;
            }
            else if (GOPSize == 2)
            {
                f.DeltaQP = 4;
            }
        }
        else if (GOPSize > 1)
        {
            f.DeltaQP = 1;
        }
        else // if (GOPSize == 1)
        {
            f.DeltaQP = f.PPyramidLayer;
        }
    }

    mfxStatus AEnc::OutputDecision(AEncFrame* OutFrame) {
        if (OutputBuffer.empty()) {
            return MFX_ERR_MORE_DATA;
        }

        ExternalFrame out = OutputBuffer.front();
        OutputBuffer.pop_front();

        //OutFrame.print();

        //delay frame removal from DPB to I/P frame
        if (out.Type == FrameType::B) {
            //do not remove on B frames
            copy(out.RemoveFromDPB.begin(), out.RemoveFromDPB.end(), std::back_inserter(RemoveFromDPBDelayed));
            out.RemoveFromDPB.clear();
        }
        else {
            //remove frame
            copy(RemoveFromDPBDelayed.begin(), RemoveFromDPBDelayed.end(), std::back_inserter(out.RemoveFromDPB));
            RemoveFromDPBDelayed.clear();
        }

        //out.print();

        *OutFrame = out;
        return MFX_ERR_NONE;
    }


    void AEnc::MarkFrameAsI(InternalFrame& f) {
        f.Type = FrameType::I;
        PocOfLastIFrame = f.POC;
    }


    void AEnc::MarkFrameAsIDR(InternalFrame& f) {
        f.Type = FrameType::IDR;
        PocOfLastIdrFrame = PocOfLastIFrame = f.POC;
    }


    void AEnc::MarkFrameAsLTR(InternalFrame& f) {
        f.LTR = true;
        f.UseLtrAsReference = true;

        m_avgMV0 = 0;
        LtrPoc = f.POC;
        m_isLtrOn = true;

        LtrScd.SetImageAndStat(f.ScdImage, f.ScdStat, ASCReference_Frame, ASCprevious_frame_data);

        f.SceneTransition = 0;
        for (mfxI32 i = 0; i < 8; i++) {
            m_sceneTranBuffer[i] = 0;
        }

        return;
    }

    void AEnc::MakeAltrDecision(InternalFrame& f) {
        if (!InitParam.ALTR){
            return;
        }

        if(f.POC == 0){
            MarkFrameAsLTR(f);
            return;
        }

        if (f.Type == FrameType::IDR && (m_isLtrOn || f.LtrOnHint)) {
            MarkFrameAsLTR(f);
            return;
        }

        if (f.SceneChanged) {
            //new LTR at SC
            MarkFrameAsLTR(f);
            return;
        }


        // Temporary, LTR frame will not be referenced. LTR still stays in DPB
        if (f.MV > 2300 || f.TSC > 1024 || (f.MV > 1024 && f.highMvCount > 6))
        {
            //don't use LTR as reference for current frame
            f.UseLtrAsReference = false;
        }
        else {
            f.UseLtrAsReference = true;
        }

    }

    void AEnc::MakeArefDecision(InternalFrame &f) {
        if (f.SceneChanged || f.Type == FrameType::IDR) {
            PocOfLastArefKeyFrame = f.POC;
        }

        if ((!InitParam.ALTR) && (f.SceneChanged || f.Type == FrameType::IDR)) {
            f.LTR = true;
        }

        if (f.Type == FrameType::P)
        {
            f.LTR = false;
            if (!m_isLtrOn || (!InitParam.ALTR))
            {
                // set P frame to Key reference frame
                uint32_t ArefKeyFrameInterval = 32;  // InitParam.MaxIDRDist;
                uint32_t minPocArefKeyFrame = PocOfLastArefKeyFrame + ArefKeyFrameInterval;
                if (f.POC >= minPocArefKeyFrame)
                {
                    f.LTR = true;
                    PocOfLastArefKeyFrame = f.POC;
                }
            }
        }
    }

    void AEnc::MarkFrameInMiniGOP(InternalFrame& f, uint32_t MiniGopSize, uint32_t MiniGopIdx ) {
        uint32_t GopTableIdx[9] = { 0/*n/a*/, 0/*1*/, 1/*2*/, 2,2/*3-4*/, 3,3,3,3/*5-8*/};
        uint32_t PyramidLayer[4/*mini GOP size 1,2,4,8*/][8] = {
            {0},
            {1,0},
            {2,1,2,0},
            {3,2,3,1,3,2,3,0}
        };
        uint32_t MiniGopType[4] = { 1, 2, 4, 8 };

        if (MiniGopSize == 0 || MiniGopSize >= sizeof(GopTableIdx)/sizeof(GopTableIdx[0])) {
            throw Error("wrong MiniGopSize");
        }

        if (MiniGopIdx >= MiniGopSize) {
            throw Error("wrong MiniGopIdx");
        }

        // 0 - P, 3 - non-reference B
        f.MiniGopSize = MiniGopSize;
        f.MiniGopIdx = MiniGopIdx;
        const uint32_t TblIdx = GopTableIdx[MiniGopSize];
        f.MiniGopType = MiniGopType[TblIdx];
        f.PyramidLayer = (MiniGopIdx == MiniGopSize - 1 ? 0 : PyramidLayer[TblIdx][MiniGopIdx]);
        if (f.Type == FrameType::UNDEF) { //may be I or IDR or UNDEF
            f.Type = (f.PyramidLayer == 0 ? FrameType::P : FrameType::B);
        }

        uint32_t PPyramid[8] = {5,4,3,2,4,3,2,1};
        if (f.Type == FrameType::I || f.Type == FrameType::IDR)
        {
            f.PPyramidLayer = 0;
            f.PPyramidIdx = 0;
        }
        else if (f.PrevType != FrameType::B && f.Type == FrameType::P)
        {
            f.PPyramidIdx = f.PPyramidIdx > 6 ? 0 : f.PPyramidIdx + 1;
            f.PPyramidLayer = PPyramid[f.PPyramidIdx];
        }
    }


    template <typename Condition>
    void AEnc::RemoveFrameFromDPB(InternalFrame &f, Condition C) {
        auto fp = std::find_if(DpbBuffer.begin(), DpbBuffer.end(), C);
        if (fp != DpbBuffer.end()) {
            f.RemoveFromDPB.push_back(fp->POC);
            DpbBuffer.erase(fp);
        }
    }


    template <typename Condition>
    void AEnc::AddFrameToRefList(InternalFrame& f, Condition C) {
        auto fp = std::min_element(DpbBuffer.begin(), DpbBuffer.end(), C);
        if (fp != DpbBuffer.end()) {
            f.RefList.push_back(fp->POC);
        }
    }

} //namespace aenc

#endif // ENABLE_ADAPTIVE_ENCODE
