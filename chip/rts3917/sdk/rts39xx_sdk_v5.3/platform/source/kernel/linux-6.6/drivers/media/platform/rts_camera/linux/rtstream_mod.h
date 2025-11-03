/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2025 Realtek Semiconductor Corp. All rights reserved.
 *
 * THIS SOFTWARE IS CONFIDENTIAL AND PROPRIETARY TO REALTEK SEMICONDUCTOR
 * CORP. DISCLOSURE, REPRODUCTION, REDISTRIBUTION, IN WHOLE OR IN PART, OF
 * THIS WORK AND ITS DERIVATIVES WITHOUT EXPRESS PERMISSION IS PROHIBITED.
 *
 * REALTEK SEMICONDUCTOR CORP. RESERVES THE RIGHT TO UPDATE, MODIFY, OR
 * DISCONTINUE THIS SOFTWARE AT ANY TIME WITHOUT NOTICE. THIS SOFTWARE IS
 * PROVIDED BY THE REGENTS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE REGENTS AND CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef _INCLUDE_RTS_RTSTREAM_MOD_H
#define _INCLUDE_RTS_RTSTREAM_MOD_H

struct rtstream_aec_op {
	int rw;

	int dump_enable;
	char dump_path[128];
};

struct rtstream_h26x_op {
	int rw;

	int EncBitrate;
	int IntraQpOffset;
	int MaxDeltaQp;
	int MinQp;
	int MaxQp;
	int IntraMinQp;
	int IntraMaxQp;
	int GOP;
	int CULevelRCEnable;
	int HvsQpEnable;
	int HvsQpScale;
	int DisableDBK;
	int BetaOffsetDiv2;
	int TcOffsetDiv2;
	int CbQpOffset;
	int CrQpOffset;
	int BgDetectLevel;
	int DeNoiseLevel;
	int RcCvbrMv0;
	int RcCvbrMv1;
	int RcCvbrMv2;
	int RcCvbrVar0;
	int RcCvbrVar1;
	int RcCvbrVar2;
	int RcCvbrMvWeightScale;
	int RcCvbrVarWeightScale;
	int IDqp;
	int PDqp;
	int PDqpFirst;
	int AnchorDqp;
	int EnCustomMD;
	int CU08MergeDeltaRate;
	int CU16MergeDeltaRate;
	int CU32MergeDeltaRate;
	int CU08IntraDeltaRate;
	int CU16IntraDeltaRate;
	int CU32IntraDeltaRate;
	int CU08InterDeltaRate;
	int CU16InterDeltaRate;
	int CU32InterDeltaRate;
	int PU04DeltaRate;
	int PU08DeltaRate;
	int PU16DeltaRate;
	int PU32DeltaRate;
};

#endif
