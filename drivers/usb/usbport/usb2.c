/*
 * PROJECT:     ReactOS USB Port Driver
 * LICENSE:     GPL-2.0+ (https://spdx.org/licenses/GPL-2.0+)
 * PURPOSE:     USBPort USB 2.0 functions
 * COPYRIGHT:   Copyright 2017 Vadim Galyant <vgal@rambler.ru>
 */

#include "usbport.h"

//#define NDEBUG
#include <debug.h>

BOOLEAN
NTAPI
USB2_AllocateCheck(IN OUT PULONG OutTimeUsed,
                   IN ULONG CalcBusTime,
                   IN ULONG LimitAllocation)
{
    ULONG BusTime;
    BOOLEAN Result = TRUE;

    BusTime = *OutTimeUsed + CalcBusTime;
    *OutTimeUsed += CalcBusTime;

    if (BusTime > LimitAllocation)
    {
        DPRINT("USB2_AllocateCheck: BusTime > LimitAllocation\n");
        Result = FALSE;
    }

    return Result;
}

USHORT
NTAPI
USB2_AddDataBitStuff(IN USHORT DataTime)
{
    return (DataTime + (DataTime / 16));
}

VOID
NTAPI
USB2_IncMicroFrame(OUT PUCHAR frame,
                   OUT PUCHAR uframe)
{
    ++*uframe;

    if (*uframe > (USB2_MICROFRAMES - 1))
    {
        *uframe = 0;
        *frame = (*frame + 1) & (USB2_FRAMES - 1);
    }
}

VOID
NTAPI
USB2_GetPrevMicroFrame(OUT PUCHAR frame,
                       OUT PUCHAR uframe)
{
    *uframe = USB2_MICROFRAMES - 1;

    if (*frame)
        --*frame;
    else
        *frame = USB2_FRAMES - 1;
}

BOOLEAN
NTAPI
USB2_CheckTtEndpointInsert(IN PUSB2_TT_ENDPOINT nextTtEndpoint,
                           IN PUSB2_TT_ENDPOINT TtEndpoint)
{
    ULONG TransferType;

    DPRINT("USB2_CheckTtEndpointInsert: nextTtEndpoint - %p, TtEndpoint - %p\n",
           nextTtEndpoint,
           TtEndpoint);

    ASSERT(TtEndpoint);

    if (TtEndpoint->CalcBusTime >= (USB2_FS_MAX_PERIODIC_ALLOCATION / 2))
    {
        DPRINT1("USB2_CheckTtEndpointInsert: Result - FALSE\n");
        return FALSE;
    }

    if (!nextTtEndpoint)
    {
        DPRINT("USB2_CheckTtEndpointInsert: Result - TRUE\n");
        return TRUE;
    }

    TransferType = TtEndpoint->TtEndpointParams.TransferType;

    if (nextTtEndpoint->ActualPeriod < TtEndpoint->ActualPeriod &&
        TransferType == USBPORT_TRANSFER_TYPE_INTERRUPT)
    {
        DPRINT("USB2_CheckTtEndpointInsert: Result - TRUE\n");
        return TRUE;
    }

    if ((nextTtEndpoint->ActualPeriod <= TtEndpoint->ActualPeriod &&
        TransferType == USBPORT_TRANSFER_TYPE_ISOCHRONOUS) ||
        nextTtEndpoint == TtEndpoint)
    {
        DPRINT("USB2_CheckTtEndpointInsert: Result - TRUE\n");
        return TRUE;
    }

    DPRINT("USB2_CheckTtEndpointInsert: Result - FALSE\n");
    return FALSE;
}

ULONG
NTAPI
USB2_GetOverhead(IN PUSB2_TT_ENDPOINT TtEndpoint)
{
    ULONG TransferType;
    ULONG Direction;
    ULONG DeviceSpeed;
    ULONG Overhead;
    ULONG HostDelay;

    TransferType = TtEndpoint->TtEndpointParams.TransferType;
    Direction = TtEndpoint->TtEndpointParams.Direction;
    DeviceSpeed = TtEndpoint->TtEndpointParams.Direction;

    HostDelay = TtEndpoint->Tt->HcExtension->HcDelayTime;

    if (DeviceSpeed == UsbHighSpeed)
    {
        if (Direction == USBPORT_TRANSFER_DIRECTION_OUT)
        {
            if (TransferType == USBPORT_TRANSFER_TYPE_ISOCHRONOUS)
                Overhead = HostDelay + USB2_HS_ISOCHRONOUS_OUT_OVERHEAD;
            else
                Overhead = HostDelay + USB2_HS_INTERRUPT_OUT_OVERHEAD;
        }
        else
        {
            if (TransferType == USBPORT_TRANSFER_TYPE_ISOCHRONOUS)
                Overhead = HostDelay + USB2_HS_ISOCHRONOUS_IN_OVERHEAD;
            else
                Overhead = HostDelay + USB2_HS_ISOCHRONOUS_OUT_OVERHEAD;
        }
    }
    else if (DeviceSpeed == UsbFullSpeed)
    {
        if (TransferType == USBPORT_TRANSFER_TYPE_ISOCHRONOUS)
            Overhead = HostDelay + USB2_FS_ISOCHRONOUS_OVERHEAD;
        else
            Overhead = HostDelay + USB2_FS_INTERRUPT_OVERHEAD;
    }
    else
    {
        Overhead = HostDelay + USB2_LS_INTERRUPT_OVERHEAD;
    }

    return Overhead;
}

VOID
NTAPI
USB2_GetHsOverhead(IN PUSB2_TT_ENDPOINT TtEndpoint,
                   IN PULONG OverheadSS,
                   IN PULONG OverheadCS)
{
    ULONG TransferType;
    ULONG Direction;
    ULONG HostDelay;

    TransferType = TtEndpoint->TtEndpointParams.TransferType;
    Direction = TtEndpoint->TtEndpointParams.Direction;

    HostDelay = TtEndpoint->Tt->HcExtension->HcDelayTime;

    if (Direction == USBPORT_TRANSFER_DIRECTION_OUT)
    {
        if (TransferType == USBPORT_TRANSFER_TYPE_ISOCHRONOUS)
        {
            *OverheadSS = HostDelay + USB2_HS_SS_ISOCHRONOUS_OUT_OVERHEAD;
            *OverheadCS = 0;
        }
        else
        {
            *OverheadSS = HostDelay + USB2_HS_SS_INTERRUPT_OUT_OVERHEAD;
            *OverheadCS = HostDelay + USB2_HS_CS_INTERRUPT_OUT_OVERHEAD;
        }
    }
    else
    {
        if (TransferType == USBPORT_TRANSFER_TYPE_ISOCHRONOUS)
        {
            *OverheadSS = HostDelay + USB2_HS_SS_ISOCHRONOUS_IN_OVERHEAD;
            *OverheadCS = HostDelay + USB2_HS_CS_ISOCHRONOUS_IN_OVERHEAD;
        }
        else
        {
            *OverheadSS = HostDelay + USB2_HS_SS_INTERRUPT_IN_OVERHEAD;
            *OverheadCS = HostDelay + USB2_HS_CS_INTERRUPT_IN_OVERHEAD;
        }

        //DPRINT("USB2_GetHsOverhead: *OverheadSS - %X, *OverheadCS - %X\n",
        //       *OverheadSS,
        //       *OverheadCS);
    }
}

ULONG
NTAPI
USB2_GetLastIsoTime(IN PUSB2_TT_ENDPOINT TtEndpoint,
                    IN ULONG Frame)
{
    PUSB2_TT_ENDPOINT nextTtEndpoint;
    ULONG Result;

    //DPRINT("USB2_GetLastIsoTime: TtEndpoint - %p, Frame - %X\n",
    //       TtEndpoint,
    //       Frame);

    nextTtEndpoint = TtEndpoint->Tt->FrameBudget[Frame].IsoEndpoint->NextTtEndpoint;

    if (nextTtEndpoint ||
        (nextTtEndpoint = TtEndpoint->Tt->FrameBudget[Frame].AltEndpoint) != NULL)
    {
        Result = nextTtEndpoint->StartTime + nextTtEndpoint->CalcBusTime;
    }
    else
    {
        Result = USB2_FS_SOF_TIME;
    }

    return Result;
}

ULONG
NTAPI
USB2_GetStartTime(IN PUSB2_TT_ENDPOINT nextTtEndpoint,
                  IN PUSB2_TT_ENDPOINT TtEndpoint,
                  IN PUSB2_TT_ENDPOINT prevTtEndpoint,
                  IN ULONG Frame)
{
    PUSB2_TT_ENDPOINT ttEndpoint;
    ULONG TransferType;

    DPRINT("USB2_GetStartTime: nextTtEndpoint - %p, TtEndpoint - %p, prevTtEndpoint - %p, Frame - %X\n",
           nextTtEndpoint,
           TtEndpoint,
           prevTtEndpoint,
           Frame);

    TransferType = TtEndpoint->TtEndpointParams.TransferType;

    if (TransferType == USBPORT_TRANSFER_TYPE_ISOCHRONOUS)
    {
        if (nextTtEndpoint)
            return nextTtEndpoint->StartTime + nextTtEndpoint->CalcBusTime;

        ttEndpoint = TtEndpoint->Tt->FrameBudget[Frame].AltEndpoint;

        if (ttEndpoint)
           return ttEndpoint->StartTime + ttEndpoint->CalcBusTime;
        else
           return USB2_FS_SOF_TIME;
    }
    else
    {
        ttEndpoint = prevTtEndpoint;

        if (ttEndpoint == TtEndpoint->Tt->FrameBudget[Frame].IntEndpoint)
            return USB2_GetLastIsoTime(TtEndpoint, Frame);
        else
            return ttEndpoint->StartTime + ttEndpoint->CalcBusTime;
    }
}

VOID
NTAPI
USB2_InitTtEndpoint(IN PUSB2_TT_ENDPOINT TtEndpoint,
                    IN UCHAR TransferType,
                    IN UCHAR Direction,
                    IN UCHAR DeviceSpeed,
                    IN USHORT Period,
                    IN USHORT MaxPacketSize,
                    IN PUSB2_TT Tt)
{
    TtEndpoint->TtEndpointParams.TransferType = TransferType;
    TtEndpoint->TtEndpointParams.Direction = Direction;
    TtEndpoint->TtEndpointParams.DeviceSpeed = DeviceSpeed;

    TtEndpoint->Period = Period;
    TtEndpoint->MaxPacketSize = MaxPacketSize;
    TtEndpoint->Tt = Tt;

    TtEndpoint->CalcBusTime = 0;
    TtEndpoint->StartTime = 0;
    TtEndpoint->ActualPeriod = 0;
    TtEndpoint->StartFrame = 0;
    TtEndpoint->StartMicroframe = 0;

    TtEndpoint->Nums.AsULONG = 0;
    TtEndpoint->NextTtEndpoint = NULL;
    TtEndpoint->Reserved2 = 0;
    TtEndpoint->PreviosPeriod = 0;
    TtEndpoint->IsPromoted = FALSE;
}

BOOLEAN
NTAPI
USB2_AllocateHS(IN PUSB2_TT_ENDPOINT TtEndpoint,
                IN LONG Frame)
{
    PUSB2_HC_EXTENSION HcExtension;
    PUSB2_TT Tt;
    ULONG TransferType;
    ULONG Direction;
    ULONG DataTime;
    ULONG DataSize;
    ULONG RemainDataTime;
    ULONG OverheadCS;
    ULONG OverheadSS;
    ULONG ix;
    USHORT PktSize;
    USHORT PktSizeBitStuff;
    UCHAR frame;
    UCHAR uframe;
    BOOL Result = TRUE;

    DPRINT("USB2_AllocateHS: TtEndpoint - %p, Frame - %X, StartFrame - %X\n",
           TtEndpoint,
           Frame,
           TtEndpoint->StartFrame);

    Tt = TtEndpoint->Tt;
    HcExtension = Tt->HcExtension;

    TransferType = TtEndpoint->TtEndpointParams.TransferType;
    Direction = TtEndpoint->TtEndpointParams.Direction;

    if (Frame == 0)
    {
        TtEndpoint->StartMicroframe =
        TtEndpoint->StartTime / USB2_FS_RAW_BYTES_IN_MICROFRAME - 1;

        DPRINT("USB2_AllocateHS: TtEndpoint->StartMicroframe - %X\n",
               TtEndpoint->StartMicroframe);
    }

    USB2_GetHsOverhead(TtEndpoint, &OverheadSS, &OverheadCS);

    if (TransferType == USBPORT_TRANSFER_TYPE_INTERRUPT)
    {
        if (Frame == 0)
        {
            TtEndpoint->Nums.NumStarts = 1;

            if ((CHAR)TtEndpoint->StartMicroframe < (USB2_MICROFRAMES - 3))
                TtEndpoint->Nums.NumCompletes = 3;
            else
                TtEndpoint->Nums.NumCompletes = 2;
        }
    }
    else
    {
        if (Direction == USBPORT_TRANSFER_DIRECTION_OUT)
        {
            DPRINT("USB2_AllocateHS: ISO UNIMPLEMENTED\n");
            ASSERT(FALSE);
        }
        else
        {
            DPRINT("USB2_AllocateHS: ISO UNIMPLEMENTED\n");
            ASSERT(FALSE);
        }
    }

    frame = TtEndpoint->StartFrame + Frame;
    uframe = TtEndpoint->StartMicroframe;

    if (TtEndpoint->StartMicroframe == 0xFF)
        USB2_GetPrevMicroFrame(&frame, &uframe);

    for (ix = 0; ix < TtEndpoint->Nums.NumStarts; ix++)
    {
        if (!USB2_AllocateCheck(&HcExtension->TimeUsed[frame][uframe],
                                OverheadSS,
                                USB2_MAX_MICROFRAME_ALLOCATION))
        {
            Result = FALSE;
        }

        if (Tt->NumStartSplits[frame][uframe] > 
            (USB2_MAX_FS_LS_TRANSACTIONS_IN_UFRAME - 1))
        {
            DPRINT1("USB2_AllocateHS: Num Start Splits - %X\n",
                    Tt->NumStartSplits[frame][uframe] + 1);

            ASSERT(FALSE);
            Result = FALSE;
        }

        ++Tt->NumStartSplits[frame][uframe];
        USB2_IncMicroFrame(&frame, &uframe);
    }

    frame = TtEndpoint->StartFrame + Frame;
    uframe = TtEndpoint->StartMicroframe + TtEndpoint->Nums.NumStarts + 1;

    for (ix = 0; ix < TtEndpoint->Nums.NumCompletes; ix++)
    {
        if (!USB2_AllocateCheck(&HcExtension->TimeUsed[frame][uframe],
                                OverheadCS,
                                USB2_MAX_MICROFRAME_ALLOCATION))
        {
            Result = FALSE;
        }

        USB2_IncMicroFrame(&frame, &uframe);
    }

    PktSize = TtEndpoint->MaxPacketSize;
    PktSizeBitStuff = USB2_AddDataBitStuff(PktSize);

    if (Direction == USBPORT_TRANSFER_DIRECTION_OUT)
    {
        frame = TtEndpoint->StartFrame + Frame;
        uframe = TtEndpoint->StartMicroframe;

        if (uframe == 0xFF)
            USB2_GetPrevMicroFrame(&frame, &uframe);

        DataTime = 0;

        for (ix = 0; ix < TtEndpoint->Nums.NumStarts; ix++)
        {
            DataSize = PktSizeBitStuff - DataTime;

            if (DataSize <= USB2_FS_RAW_BYTES_IN_MICROFRAME)
                DataTime = DataSize;
            else
                DataTime = USB2_FS_RAW_BYTES_IN_MICROFRAME;

            DPRINT("USB2_AllocateHS: ix - %X, frame - %X, uframe - %X, TimeUsed - %X\n",
                   ix,
                   frame,
                   uframe,
                   HcExtension->TimeUsed[frame][uframe]);

            if (!USB2_AllocateCheck(&HcExtension->TimeUsed[frame][uframe],
                                    DataTime,
                                    USB2_MAX_MICROFRAME_ALLOCATION))
            {
                Result = FALSE;
            }

            USB2_IncMicroFrame(&frame, &uframe);
            DataTime += USB2_FS_RAW_BYTES_IN_MICROFRAME;
        }
    }
    else
    {
        frame = TtEndpoint->StartFrame + Frame;
        uframe = TtEndpoint->StartMicroframe + TtEndpoint->Nums.NumStarts + 1;

        for (ix = 0; ix < TtEndpoint->Nums.NumCompletes; ix++)
        {
            if (Tt->TimeCS[frame][uframe] < USB2_FS_RAW_BYTES_IN_MICROFRAME)
            {
                RemainDataTime = USB2_FS_RAW_BYTES_IN_MICROFRAME -
                                 Tt->TimeCS[frame][uframe];

                if (RemainDataTime >= PktSizeBitStuff)
                {
                    DataTime = PktSizeBitStuff;
                }
                else if (RemainDataTime > 0)
                {
                    DataTime = RemainDataTime;
                }
                else
                {
                    DataTime = 0;
                }

                if (!USB2_AllocateCheck(&HcExtension->TimeUsed[frame][uframe],
                                        DataTime,
                                        USB2_MAX_MICROFRAME_ALLOCATION))
                {
                    Result = FALSE;
                }
            }

            if (PktSizeBitStuff < USB2_FS_RAW_BYTES_IN_MICROFRAME)
                Tt->TimeCS[frame][uframe] += PktSizeBitStuff;
            else
                Tt->TimeCS[frame][uframe] += USB2_FS_RAW_BYTES_IN_MICROFRAME;

            USB2_IncMicroFrame(&frame, &uframe);
        }
    }

    DPRINT("USB2_AllocateHS: Result - %X\n", Result);
    return Result;
}

VOID
NTAPI
USB2_DeallocateHS(IN PUSB2_TT_ENDPOINT TtEndpoint,
                  IN ULONG Frame)
{
    PUSB2_TT Tt;
    PUSB2_HC_EXTENSION HcExtension;
    ULONG OverheadCS;
    ULONG OverheadSS;
    ULONG Direction;
    ULONG ix;
    ULONG CurrentDataTime;
    ULONG RemainDataTime;
    ULONG DataTime;
    ULONG DataSize;
    USHORT PktSize;
    USHORT PktSizeBitStuff;
    UCHAR uframe;
    UCHAR frame;

    DPRINT("USB2_DeallocateHS: TtEndpoint - %p, Frame - %X\n",
           TtEndpoint,
           Frame);

    Tt = TtEndpoint->Tt;
    HcExtension = Tt->HcExtension;

    USB2_GetHsOverhead(TtEndpoint, &OverheadSS, &OverheadCS);

    frame = TtEndpoint->StartFrame + Frame;
    uframe = TtEndpoint->StartMicroframe;

    if (TtEndpoint->StartMicroframe == 0xFF)
    {
        USB2_GetPrevMicroFrame(&frame, &uframe);
    }

    for (ix = 0; ix < TtEndpoint->Nums.NumStarts; ix++)
    {
        HcExtension->TimeUsed[frame][uframe] -= OverheadSS;
        --Tt->NumStartSplits[frame][uframe];
        USB2_IncMicroFrame(&frame, &uframe);
    }

    frame = TtEndpoint->StartFrame + Frame;
    uframe = TtEndpoint->StartMicroframe + TtEndpoint->Nums.NumStarts + 1;

    for (ix = 0; ix < TtEndpoint->Nums.NumCompletes; ix++)
    {
        HcExtension->TimeUsed[frame][uframe] -= OverheadCS;
        USB2_IncMicroFrame(&frame, &uframe);
    }

    Direction = TtEndpoint->TtEndpointParams.Direction;
    PktSize = TtEndpoint->MaxPacketSize;
    PktSizeBitStuff = USB2_AddDataBitStuff(PktSize);

    if (Direction == USBPORT_TRANSFER_DIRECTION_OUT)
    {
        frame = TtEndpoint->StartFrame + Frame;
        uframe = TtEndpoint->StartMicroframe;

        if (TtEndpoint->StartMicroframe == 0xFF)
        {
            USB2_GetPrevMicroFrame(&frame, &uframe);
        }

        DataTime = 0;

        for (ix = 0; ix < TtEndpoint->Nums.NumStarts; ix++)
        {
            DataSize = PktSizeBitStuff - DataTime;

            if (DataSize <= USB2_FS_RAW_BYTES_IN_MICROFRAME)
            {
                CurrentDataTime = PktSizeBitStuff - DataTime;
            }
            else
            {
                CurrentDataTime = USB2_FS_RAW_BYTES_IN_MICROFRAME;
            }

            HcExtension->TimeUsed[frame][uframe] -= CurrentDataTime;
            USB2_IncMicroFrame(&frame, &uframe);
            DataTime += USB2_FS_RAW_BYTES_IN_MICROFRAME;
        }
    }
    else
    {
        frame = TtEndpoint->StartFrame + Frame;
        uframe = TtEndpoint->StartMicroframe + TtEndpoint->Nums.NumStarts + 1;

        for (ix = 0; ix < TtEndpoint->Nums.NumCompletes; ix++)
        {
            if (PktSizeBitStuff >= USB2_FS_RAW_BYTES_IN_MICROFRAME)
            {
                CurrentDataTime = USB2_FS_RAW_BYTES_IN_MICROFRAME;
            }
            else
            {
                CurrentDataTime = PktSizeBitStuff;
            }

            Tt->TimeCS[frame][uframe] -= CurrentDataTime;

            if (Tt->TimeCS[frame][uframe] < USB2_FS_RAW_BYTES_IN_MICROFRAME)
            {
                RemainDataTime = USB2_FS_RAW_BYTES_IN_MICROFRAME -
                                 Tt->TimeCS[frame][uframe];

                if (RemainDataTime >= PktSizeBitStuff)
                    RemainDataTime = PktSizeBitStuff;

                HcExtension->TimeUsed[frame][uframe] -= RemainDataTime;
            }

            USB2_IncMicroFrame(&frame, &uframe);
        }
    }

    return;
}

BOOLEAN
NTAPI
USB2_MoveTtEndpoint(IN PUSB2_TT_ENDPOINT TtEndpoint,
                    IN USHORT BusTime,
                    IN PUSB2_REBALANCE Rebalance,
                    IN ULONG RebalanceListEntries,
                    OUT BOOLEAN * OutResult)
{
    ULONG EndBusTime;
    ULONG TransferType;
    ULONG Num;
    UCHAR ix;

    DPRINT("USB2_MoveTtEndpoint: TtEndpoint - %p, BusTime - %X\n",
           TtEndpoint,
           BusTime);

    *OutResult = TRUE;

    for (Num = 0; Rebalance->RebalanceEndpoint[Num]; Num++)
    {
        if (Rebalance->RebalanceEndpoint[Num] == TtEndpoint)
            break;
    }

    DPRINT("USB2_MoveTtEndpoint: Num - %X\n", Num);

    TransferType = TtEndpoint->TtEndpointParams.TransferType;

    if (Rebalance->RebalanceEndpoint[Num] &&
        TtEndpoint->TtEndpointParams.EndpointMoved == TRUE &&
        (TransferType != USBPORT_TRANSFER_TYPE_INTERRUPT || BusTime >= 0))
    {
        DPRINT("USB2_MoveTtEndpoint: result - FALSE\n");
        return FALSE;
    }

    for (ix = 0;
         (TtEndpoint->StartFrame + ix) < USB2_FRAMES;
         ix += TtEndpoint->ActualPeriod)
    {
        USB2_DeallocateHS(TtEndpoint, ix);
    }

    TtEndpoint->StartTime += BusTime;

    EndBusTime = TtEndpoint->StartTime + TtEndpoint->CalcBusTime;

    if (EndBusTime > USB2_FS_MAX_PERIODIC_ALLOCATION)
    {
        DPRINT("USB2_MoveTtEndpoint: EndBusTime is too large!\n");
        *OutResult = FALSE;
    }

    TtEndpoint->TtEndpointParams.EndpointMoved = TRUE;

    if (Rebalance->RebalanceEndpoint[Num] == NULL)
    {
        if (Num >= RebalanceListEntries)
        {
            DPRINT("USB2_MoveTtEndpoint: Too many changes!\n");
            *OutResult = FALSE;
        }
        else
        {
            Rebalance->RebalanceEndpoint[Num] = TtEndpoint;
            Rebalance->RebalanceEndpoint[Num + 1] = NULL;
        }
    }

    for (ix = 0;
         (TtEndpoint->StartFrame + ix) < USB2_FRAMES;
         ix += TtEndpoint->ActualPeriod)
    {
        if (!USB2_AllocateHS(TtEndpoint, ix))
        {
            DPRINT("USB2_MoveTtEndpoint: OutResult - FALSE\n");
            OutResult = FALSE;
        }
    }

    DPRINT("USB2_MoveTtEndpoint: result - TRUE\n");
    return TRUE;
}

BOOLEAN
NTAPI
USB2_CommonFrames(IN PUSB2_TT_ENDPOINT NextTtEndpoint,
                  IN PUSB2_TT_ENDPOINT TtEndpoint)
{
    UCHAR Frame;

    DPRINT("USB2_CommonFrames: \n");

    if (NextTtEndpoint->ActualPeriod == ENDPOINT_INTERRUPT_1ms ||
        TtEndpoint->ActualPeriod == ENDPOINT_INTERRUPT_1ms)
    {
        return TRUE;
    }

    if (NextTtEndpoint->ActualPeriod < TtEndpoint->ActualPeriod)
    {
        Frame = TtEndpoint->StartFrame % TtEndpoint->ActualPeriod;
    }
    else
    {
        Frame = NextTtEndpoint->StartFrame % TtEndpoint->ActualPeriod;
    }

    return (Frame == TtEndpoint->StartFrame);
}

VOID
NTAPI
USB2_ConvertFrame(IN UCHAR Frame,
                  IN UCHAR Microframe,
                  OUT PUCHAR HcFrame,
                  OUT PUCHAR HcMicroframe)
{
    DPRINT("USB2_ConvertFrame: Frame - %x, Microframe - %x\n",
           Frame,
           Microframe);

    if (Microframe == 0xFF)
    {
        *HcFrame = Frame;
        *HcMicroframe = 0;
    }

    if (Microframe >= 0 &&
        Microframe <= (USB2_MICROFRAMES - 2))
    {
        *HcFrame = Frame;
        *HcMicroframe = Microframe + 1;
    }

    if (Microframe == (USB2_MICROFRAMES - 1))
    {
        *HcFrame = Frame + 1;
        *HcMicroframe = 0;
    }
}

UCHAR
NTAPI
USB2_GetSMASK(IN PUSB2_TT_ENDPOINT TtEndpoint)
{
    ULONG ix;
    UCHAR SMask = 0;
    UCHAR HcFrame;
    UCHAR HcMicroFrame;

    if (TtEndpoint->TtEndpointParams.DeviceSpeed == UsbHighSpeed)
    {
        SMask = (1 << TtEndpoint->StartMicroframe);
    }
    else
    {
        USB2_ConvertFrame(TtEndpoint->StartFrame,
                          TtEndpoint->StartMicroframe,
                          &HcFrame,
                          &HcMicroFrame);

        for (ix = 0; ix < TtEndpoint->Nums.NumStarts; ix++)
        {
            SMask |= (1 << HcMicroFrame);
            HcMicroFrame++;
        }
    }

    return SMask;
}

UCHAR
NTAPI
USB2_GetCMASK(IN PUSB2_TT_ENDPOINT TtEndpoint)
{
    ULONG NumCompletes;
    ULONG TransferType;
    ULONG DeviceSpeed;
    ULONG Direction;
    UCHAR Result;
    UCHAR MicroFrameCS;
    UCHAR HcFrame;
    UCHAR HcMicroFrame;
    UCHAR MaskCS = 0;
    static const UCHAR CMASKS[USB2_MICROFRAMES] = {
      0x1C, 0x38, 0x70, 0xE0, 0xC1, 0x83, 0x07, 0x0E 
    };   

    TransferType = TtEndpoint->TtEndpointParams.TransferType;
    DeviceSpeed = TtEndpoint->TtEndpointParams.DeviceSpeed;
    Direction = TtEndpoint->TtEndpointParams.Direction;

    if (DeviceSpeed == UsbHighSpeed)
    {
        return 0;
    }

    if (TransferType == USBPORT_TRANSFER_TYPE_INTERRUPT)
    {
        USB2_ConvertFrame(TtEndpoint->StartFrame,
                          TtEndpoint->StartMicroframe,
                          &HcFrame,
                          &HcMicroFrame);

        Result = CMASKS[HcMicroFrame];
    }
    else
    {
        if (Direction == USBPORT_TRANSFER_DIRECTION_OUT)
        {
            return 0;
        }

        USB2_ConvertFrame(TtEndpoint->StartFrame,
                          TtEndpoint->StartMicroframe,
                          &HcFrame,
                          &HcMicroFrame);

        NumCompletes = TtEndpoint->Nums.NumCompletes;

        for (MicroFrameCS = HcMicroFrame + 2;
             MicroFrameCS < USB2_MICROFRAMES;
             MicroFrameCS++)
        {
            MaskCS |= (1 << MicroFrameCS);
            NumCompletes--;

            if (!NumCompletes)
            {
                return MaskCS;
            }
        }

        for (; NumCompletes; NumCompletes--)
        {
            MaskCS |= (1 << (MicroFrameCS - USB2_MICROFRAMES));
        }

        Result = MaskCS;
    }

    return Result;
}

VOID
NTAPI
USB2_RebalanceEndpoint(IN PDEVICE_OBJECT FdoDevice,
                       IN PLIST_ENTRY List)
{
    DPRINT1("USB2_RebalanceEndpoint: UNIMPLEMENTED. FIXME\n");
    ASSERT(FALSE);
}

VOID
NTAPI
USB2_Rebalance(IN PDEVICE_OBJECT FdoDevice,
               IN PLIST_ENTRY RebalanceList)
{
    PUSBPORT_ENDPOINT_PROPERTIES EndpointProperties;
    PUSBPORT_ENDPOINT Endpoint;
    PLIST_ENTRY Entry;
    LIST_ENTRY BalanceListInt1;
    LIST_ENTRY BalanceListInt2;
    ULONG TransferType;
    ULONG ScheduleOffset;
    UCHAR SMask;
    UCHAR CMask;
    UCHAR ActualPeriod;

    DPRINT("USB2_Rebalance: FdoDevice - %p, RebalanceList - %p\n",
           FdoDevice,
           RebalanceList);

    InitializeListHead(&BalanceListInt1);
    InitializeListHead(&BalanceListInt2);

    while (!IsListEmpty(RebalanceList))
    {
        Entry = RebalanceList->Flink;

        Endpoint = CONTAINING_RECORD(Entry,
                                     USBPORT_ENDPOINT,
                                     RebalanceLink.Flink);

        DPRINT("USBPORT_Rebalance: Entry - %p, Endpoint - %p\n",
               Entry,
               Endpoint);

        RemoveHeadList(RebalanceList);
        Entry->Flink = NULL;
        Entry->Blink = NULL;

        SMask = USB2_GetSMASK(Endpoint->TtEndpoint);
        CMask = USB2_GetCMASK(Endpoint->TtEndpoint);

        ScheduleOffset = Endpoint->TtEndpoint->StartFrame;
        ActualPeriod = Endpoint->TtEndpoint->ActualPeriod;

        EndpointProperties = &Endpoint->EndpointProperties;
        TransferType = EndpointProperties->TransferType;

        switch (TransferType)
        {
            case USBPORT_TRANSFER_TYPE_ISOCHRONOUS:
                DPRINT("USBPORT_Rebalance: USBPORT_TRANSFER_TYPE_ISOCHRONOUS. FIXME\n");
                ASSERT(FALSE);
                break;

            case USBPORT_TRANSFER_TYPE_INTERRUPT:
                if (SMask != EndpointProperties->InterruptScheduleMask ||
                    CMask != EndpointProperties->SplitCompletionMask || 
                    ScheduleOffset != EndpointProperties->ScheduleOffset ||
                    ActualPeriod != EndpointProperties->Period)
                {
                    if (ActualPeriod == EndpointProperties->Period &&
                        ScheduleOffset == EndpointProperties->ScheduleOffset)
                    {
                        InsertTailList(&BalanceListInt1, Entry);
                    }
                    else
                    {
                        InsertTailList(&BalanceListInt2, Entry);
                    }
                }
                break;

            default:
                ASSERT(FALSE);
                break;
        }
    }

    USB2_RebalanceEndpoint(FdoDevice, &BalanceListInt2);
    USB2_RebalanceEndpoint(FdoDevice, &BalanceListInt1);
    //USB2_RebalanceEndpoint(FdoDevice, &BalanceListIso);
}

BOOLEAN
NTAPI
USB2_DeallocateEndpointBudget(IN PUSB2_TT_ENDPOINT TtEndpoint,
                              IN PUSB2_REBALANCE Rebalance,
                              IN PULONG RebalanceListEntries,
                              IN ULONG MaxFrames)
{
    PUSB2_TT Tt;
    PUSB2_HC_EXTENSION HcExtension;
    ULONG Speed;
    ULONG StartMicroframe;
    ULONG ix;

    UCHAR frame;
    UCHAR uframe;

    DPRINT("USB2_DeallocateEndpointBudget: TtEndpoint - %p, MaxFrames - %X\n",
           TtEndpoint,
           MaxFrames);

    if (TtEndpoint->CalcBusTime == 0)
    {
        DPRINT("USB2_DeallocateEndpointBudget: endpoint not allocated\n");//error((int)"endpoint not allocated");
        return FALSE;
    }

    Tt = TtEndpoint->Tt;
    HcExtension = Tt->HcExtension;

    Speed = TtEndpoint->TtEndpointParams.DeviceSpeed;
    DPRINT("USB2_DeallocateEndpointBudget: DeviceSpeed - %X\n", Speed);

    StartMicroframe = TtEndpoint->StartFrame * USB2_MICROFRAMES +
                      TtEndpoint->StartMicroframe;

    if (Speed == UsbHighSpeed)
    {
        for (ix = StartMicroframe;
             ix < USB2_MAX_MICROFRAMES;
             ix += TtEndpoint->ActualPeriod)
        {
            frame = ix / USB2_MICROFRAMES;
            uframe = ix % (USB2_MICROFRAMES - 1);

            HcExtension->TimeUsed[frame][uframe] -= TtEndpoint->CalcBusTime;
        }

        TtEndpoint->CalcBusTime = 0;

        DPRINT("USB2_DeallocateEndpointBudget: return TRUE\n");
        return TRUE;
    }

    /* Speed != UsbHighSpeed (FS/LS) */

    DPRINT("USB2_DeallocateEndpointBudget: UNIMPLEMENTED FIXME\n");
    ASSERT(FALSE);

    TtEndpoint->CalcBusTime = 0;
    DPRINT("USB2_DeallocateEndpointBudget: return TRUE\n");
    return TRUE;
}

BOOLEAN
NTAPI
USB2_AllocateTimeForEndpoint(IN PUSB2_TT_ENDPOINT TtEndpoint,
                             IN PUSB2_REBALANCE Rebalance,
                             IN PULONG RebalanceListEntries)
{
    PUSB2_TT Tt;
    PUSB2_HC_EXTENSION HcExtension;
    ULONG Speed;
    ULONG TimeUsed;
    ULONG MinTimeUsed;
    ULONG ix;
    ULONG frame;
    ULONG uframe;
    ULONG Microframe;
    ULONG TransferType;
    ULONG Overhead;
    ULONG LatestStart;
    PUSB2_TT_ENDPOINT prevEndpoint;
    PUSB2_TT_ENDPOINT nextEndpoint;
    PUSB2_TT_ENDPOINT IntEndpoint;
    ULONG StartTime;
    ULONG calcBusTime;
    BOOLEAN Result = TRUE;

    DPRINT("USB2_AllocateTimeForEndpoint: TtEndpoint - %p\n", TtEndpoint);

    Tt = TtEndpoint->Tt;
    HcExtension = Tt->HcExtension;

    TtEndpoint->Nums.NumStarts = 0;
    TtEndpoint->Nums.NumCompletes = 0;

    TtEndpoint->StartFrame = 0;
    TtEndpoint->StartMicroframe = 0;

    if (TtEndpoint->CalcBusTime)
    {
        DPRINT("USB2_AllocateTimeForEndpoint: TtEndpoint already allocated!\n");
        return FALSE;
    }

    Speed = TtEndpoint->TtEndpointParams.DeviceSpeed;

    if (Speed == UsbHighSpeed)
    {
        if (TtEndpoint->Period > USB2_MAX_MICROFRAMES)
            TtEndpoint->ActualPeriod = USB2_MAX_MICROFRAMES;
        else
            TtEndpoint->ActualPeriod = TtEndpoint->Period;

        MinTimeUsed = HcExtension->TimeUsed[0][0];

        for (ix = 1; ix < TtEndpoint->ActualPeriod; ix++)
        {
            frame = ix / USB2_MICROFRAMES;
            uframe = ix % (USB2_MICROFRAMES - 1);

            TimeUsed = HcExtension->TimeUsed[frame][uframe];

            if (TimeUsed < MinTimeUsed)
            {
                MinTimeUsed = TimeUsed;
                TtEndpoint->StartFrame = frame;
                TtEndpoint->StartMicroframe = uframe;
            }
        }

        TtEndpoint->CalcBusTime = USB2_GetOverhead(TtEndpoint) +
                                  USB2_AddDataBitStuff(TtEndpoint->MaxPacketSize);

        DPRINT("USB2_AllocateTimeForEndpoint: StartFrame - %X, StartMicroframe - %X, CalcBusTime - %X\n",
               TtEndpoint->StartFrame,
               TtEndpoint->StartMicroframe,
               TtEndpoint->CalcBusTime);

        Microframe = TtEndpoint->StartFrame * USB2_MICROFRAMES +
                     TtEndpoint->StartMicroframe;

        if (Microframe >= USB2_MAX_MICROFRAMES)
        {
            DPRINT("USB2_AllocateTimeForEndpoint: Microframe >= 256. Result - TRUE\n");
            return TRUE;
        }

        for (ix = Microframe;
             ix < USB2_MAX_MICROFRAMES;
             ix += TtEndpoint->ActualPeriod)
        {
            frame = ix / USB2_MICROFRAMES;
            uframe = ix % (USB2_MICROFRAMES - 1);

            DPRINT("USB2_AllocateTimeForEndpoint: frame - %X, uframe - %X, TimeUsed[f][uf] - %X\n",
                   frame,
                   uframe,
                   HcExtension->TimeUsed[frame][uframe]);

            if (!USB2_AllocateCheck(&HcExtension->TimeUsed[frame][uframe],
                                    TtEndpoint->CalcBusTime,
                                    USB2_MAX_MICROFRAME_ALLOCATION))
            {
                DPRINT("USB2_AllocateTimeForEndpoint: Result = FALSE\n");
                Result = FALSE;
            }
        }

        if (!Result)
        {
            for (ix = Microframe;
                 ix < USB2_MAX_MICROFRAMES;
                 ix += TtEndpoint->ActualPeriod)
            {
                frame = ix / USB2_MICROFRAMES;
                uframe = ix % (USB2_MICROFRAMES - 1);

                HcExtension->TimeUsed[frame][uframe] -= TtEndpoint->CalcBusTime;
            }
        }

        DPRINT("USB2_AllocateTimeForEndpoint: Result - TRUE\n");
        return TRUE;
    }

    /* Speed != UsbHighSpeed (FS/LS) */

    if (TtEndpoint->Period > USB2_FRAMES)
        TtEndpoint->ActualPeriod = USB2_FRAMES;
    else
        TtEndpoint->ActualPeriod = TtEndpoint->Period;

    MinTimeUsed = Tt->FrameBudget[0].TimeUsed;

    for (ix = 1; ix < TtEndpoint->ActualPeriod; ix++)
    {
        if ((Tt->FrameBudget[ix].TimeUsed) < MinTimeUsed)
        {
            MinTimeUsed = Tt->FrameBudget[ix].TimeUsed;
            TtEndpoint->StartFrame = ix;
        }
    }

    TransferType = TtEndpoint->TtEndpointParams.TransferType;

    if (TransferType == USBPORT_TRANSFER_TYPE_ISOCHRONOUS)
    {
        if (Speed == UsbFullSpeed)
        {
            Overhead = USB2_FS_ISOCHRONOUS_OVERHEAD + Tt->DelayTime;
        }
        else
        {
            DPRINT("USB2_AllocateTimeForEndpoint: ISO can not be on a LS bus!\n");
            return FALSE;
        }
    }
    else
    {
        if (Speed == UsbFullSpeed)
            Overhead = USB2_FS_INTERRUPT_OVERHEAD + Tt->DelayTime;
        else
            Overhead = USB2_LS_INTERRUPT_OVERHEAD + Tt->DelayTime;
    }

    if (Speed == UsbLowSpeed)
    {
        TtEndpoint->CalcBusTime = TtEndpoint->MaxPacketSize * 8 + Overhead;
    }
    else
    {
        TtEndpoint->CalcBusTime = TtEndpoint->MaxPacketSize + Overhead;
    }

    LatestStart = USB2_HUB_DELAY + USB2_FS_SOF_TIME;

    for (ix = 0;
         (TtEndpoint->StartFrame + ix) < USB2_FRAMES;
         ix += TtEndpoint->ActualPeriod)
    {
        frame = TtEndpoint->StartFrame + ix;

        if (Tt->FrameBudget[frame].AltEndpoint &&
            TtEndpoint->CalcBusTime >= (USB2_FS_MAX_PERIODIC_ALLOCATION / 2))
        {
            DPRINT("USB2_AllocateTimeForEndpoint: return FALSE\n");
            return FALSE;
        }

        if (TransferType == USBPORT_TRANSFER_TYPE_ISOCHRONOUS)
            prevEndpoint = Tt->FrameBudget[frame].IsoEndpoint;
        else
            prevEndpoint = Tt->FrameBudget[frame].IntEndpoint;

        for (nextEndpoint = prevEndpoint->NextTtEndpoint;
             nextEndpoint;
             nextEndpoint = nextEndpoint->NextTtEndpoint)
        {
            if (USB2_CheckTtEndpointInsert(nextEndpoint, TtEndpoint))
            {
                break;
            }

            prevEndpoint = nextEndpoint;
        }

        StartTime = USB2_GetStartTime(nextEndpoint,
                                      TtEndpoint,
                                      prevEndpoint,
                                      frame);

        if (StartTime > LatestStart)
            LatestStart = StartTime;
    }

    TtEndpoint->StartTime = LatestStart;

    if ((LatestStart + TtEndpoint->CalcBusTime) > USB2_FS_MAX_PERIODIC_ALLOCATION)
    {
        TtEndpoint->CalcBusTime = 0;
        DPRINT("USB2_AllocateTimeForEndpoint: return FALSE\n");
        return FALSE;
    }

    for (ix = 0, frame = -TtEndpoint->StartFrame;
         ix < USB2_FRAMES;
         ix++, frame++)
    {
        DPRINT("USB2_AllocateTimeForEndpoint: ix - %X, frame - %X, StartFrame - %X\n",
               ix,
               frame,
               TtEndpoint->StartFrame);

        if (TransferType == USBPORT_TRANSFER_TYPE_ISOCHRONOUS)
        {
            DPRINT1("USB2_AllocateTimeForEndpoint: Iso Ep UNIMPLEMENTED. FIXME\n");
            ASSERT(FALSE);
        }
        else
        {
            IntEndpoint = Tt->FrameBudget[ix].IntEndpoint;
            nextEndpoint = IntEndpoint->NextTtEndpoint;

            for (nextEndpoint = IntEndpoint->NextTtEndpoint;
                 nextEndpoint;
                 nextEndpoint = nextEndpoint->NextTtEndpoint)
            {
                if (USB2_CheckTtEndpointInsert(nextEndpoint, TtEndpoint))
                    break;
                IntEndpoint = nextEndpoint;
            }

            if ((frame % TtEndpoint->ActualPeriod) == 0)
            {
                calcBusTime = 0;
            }
            else
            {
                if (nextEndpoint)
                {
                    calcBusTime = LatestStart + TtEndpoint->CalcBusTime -
                                  nextEndpoint->StartTime;
                }
                else
                {
                    calcBusTime = TtEndpoint->CalcBusTime;
                }

                if (calcBusTime > 0)
                {
                    TimeUsed = Tt->FrameBudget[ix].TimeUsed;

                    if (!USB2_AllocateCheck(&TimeUsed,
                                            calcBusTime,
                                            USB2_FS_MAX_PERIODIC_ALLOCATION))
                    {
                        DPRINT("USB2_AllocateTimeForEndpoint: Result = FALSE\n");
                        Result = FALSE;
                    }
                }
            }

            if (nextEndpoint != TtEndpoint)
            {
                if ((frame % TtEndpoint->ActualPeriod) == 0)
                {
                    if (frame == 0)
                    {
                        DPRINT("USB2_AllocateTimeForEndpoint: frame == 0\n");
                        TtEndpoint->NextTtEndpoint = nextEndpoint;
                    }

                    IntEndpoint->NextTtEndpoint = TtEndpoint;

                    DPRINT("USB2_AllocateTimeForEndpoint: TtEndpoint - %p, nextEndpoint - %p\n",
                           TtEndpoint,
                           nextEndpoint);
                }

                if (calcBusTime > 0)
                {
                    BOOLEAN IsMoved;
                    BOOLEAN MoveResult;

                    DPRINT("USB2_AllocateTimeForEndpoint: nextEndpoint - %p, calcBusTime - %X\n",
                           nextEndpoint,
                           calcBusTime);

                    for (;
                         nextEndpoint;
                         nextEndpoint = nextEndpoint->NextTtEndpoint)
                    {
                        MoveResult = USB2_MoveTtEndpoint(nextEndpoint,
                                                         calcBusTime,
                                                         Rebalance,
                                                         *RebalanceListEntries,
                                                         &IsMoved);

                        if (!IsMoved)
                        {
                            DPRINT("USB2_AllocateTimeForEndpoint: Result = FALSE\n");
                            Result = FALSE;
                        }

                        if (!MoveResult)
                            break;
                    }
                }
            }
        }

        if ((frame % TtEndpoint->ActualPeriod) == 0)
        {
            if (!USB2_AllocateHS(TtEndpoint, frame))
            {
                DPRINT1("USB2_AllocateTimeForEndpoint: USB2_AllocateHS return FALSE\n");
                Result = FALSE;
            }

            Tt->FrameBudget[ix].TimeUsed += TtEndpoint->CalcBusTime;
        }

        if (Result == FALSE)
        {
            USB2_DeallocateEndpointBudget(TtEndpoint,
                                          Rebalance,
                                          RebalanceListEntries,
                                          ix + 1);

            DPRINT("USB2_AllocateTimeForEndpoint: return FALSE\n");
            return FALSE;
        }
    }

    DPRINT("USB2_AllocateTimeForEndpoint: Result - %X\n", Result);
    return Result;
}

BOOLEAN
NTAPI
USB2_PromotePeriods(IN PUSB2_TT_ENDPOINT TtEndpoint,
                    IN PUSB2_REBALANCE Rebalance,
                    IN PULONG RebalanceListEntries)
{
    DPRINT1("USB2_PromotePeriods: UNIMPLEMENTED. FIXME\n");
    ASSERT(FALSE);
    return FALSE;
}

VOID
NTAPI
USBPORT_UpdateAllocatedBwTt(IN PUSB2_TT_EXTENSION TtExtension)
{
    ULONG BusBandwidth;
    ULONG NewBusBandwidth;
    ULONG MaxBusBandwidth = 0;
    ULONG MinBusBandwidth;
    ULONG ix;

    DPRINT("USBPORT_UpdateAllocatedBwTt: TtExtension - %p\n", TtExtension);

    BusBandwidth = TtExtension->BusBandwidth;
    MinBusBandwidth = BusBandwidth;

    for (ix = 0; ix < USB2_FRAMES; ix++)
    {
        NewBusBandwidth = BusBandwidth - TtExtension->Bandwidth[ix];

        if (NewBusBandwidth > MaxBusBandwidth)
            MaxBusBandwidth = NewBusBandwidth;

        if (NewBusBandwidth < MinBusBandwidth)
            MinBusBandwidth = NewBusBandwidth;
    }

    TtExtension->MaxBandwidth = MaxBusBandwidth;

    if (MinBusBandwidth == BusBandwidth)
        TtExtension->MinBandwidth = 0;
    else
        TtExtension->MinBandwidth = MinBusBandwidth;
}

BOOLEAN
NTAPI
USBPORT_AllocateBandwidthUSB2(IN PDEVICE_OBJECT FdoDevice,
                              IN PUSBPORT_ENDPOINT Endpoint)
{
    PUSBPORT_DEVICE_EXTENSION FdoExtension;
    PUSBPORT_ENDPOINT_PROPERTIES EndpointProperties;
    PUSB2_TT_EXTENSION TtExtension;
    ULONG TransferType;
    PUSB2_REBALANCE Rebalance;
    LIST_ENTRY RebalanceList;
    ULONG RebalanceListEntries;
    PUSB2_TT_ENDPOINT TtEndpoint;
    PUSB2_TT_ENDPOINT RebalanceTtEndpoint;
    PUSB2_TT Tt;
    USB_DEVICE_SPEED DeviceSpeed;
    ULONG Period;
    ULONG AllocedBusTime;
    ULONG EndpointBandwidth;
    ULONG ScheduleOffset;
    ULONG Factor;
    ULONG ix;
    ULONG n;
    BOOLEAN Direction;
    UCHAR SMask;
    UCHAR CMask;
    UCHAR ActualPeriod;
    BOOLEAN Result;

    DPRINT("USBPORT_AllocateBandwidthUSB2: FdoDevice - %p, Endpoint - %p\n",
           FdoDevice,
           Endpoint);

    EndpointProperties = &Endpoint->EndpointProperties;
    EndpointProperties->ScheduleOffset = 0;

    if (Endpoint->Flags & ENDPOINT_FLAG_ROOTHUB_EP0)
    {
        DPRINT("USBPORT_AllocateBandwidthUSB2: ENDPOINT_FLAG_ROOTHUB_EP0\n");
        return TRUE;
    }

    FdoExtension = FdoDevice->DeviceExtension;

    TransferType = EndpointProperties->TransferType;
    DPRINT("USBPORT_AllocateBandwidthUSB2: TransferType - %X\n", TransferType);

    if (TransferType == USBPORT_TRANSFER_TYPE_CONTROL ||
        TransferType == USBPORT_TRANSFER_TYPE_BULK)
    {
        return TRUE;
    }

    if (Endpoint->TtExtension)
        TtExtension = Endpoint->TtExtension;
    else
        TtExtension = NULL;

    InitializeListHead(&RebalanceList);

    Rebalance = ExAllocatePoolWithTag(NonPagedPool,
                                      sizeof(USB2_REBALANCE),
                                      USB_PORT_TAG);

    DPRINT("USBPORT_AllocateBandwidthUSB2: Rebalance - %p, TtExtension - %p\n",
           Rebalance,
           TtExtension);

    if (Rebalance)
    {
        RtlZeroMemory(Rebalance, sizeof(USB2_REBALANCE));

        TtEndpoint = Endpoint->TtEndpoint;
        TtEndpoint->Endpoint = Endpoint;

        Direction = EndpointProperties->Direction == USBPORT_TRANSFER_DIRECTION_OUT;
        DeviceSpeed = EndpointProperties->DeviceSpeed;

        switch (DeviceSpeed)
        {
            case UsbLowSpeed:
            case UsbFullSpeed:
            {
                Tt = &TtExtension->Tt;

                Period = USB2_FRAMES;

                while (Period > 0 && Period > EndpointProperties->Period)
                {
                    Period >>= 1;
                }

                DPRINT("USBPORT_AllocateBandwidthUSB2: Period - %X\n", Period);
                break;
            }

            case UsbHighSpeed:
            {
                Tt = &FdoExtension->Usb2Extension->HcTt;

                if (EndpointProperties->Period > USB2_MAX_MICROFRAMES)
                    Period = USB2_MAX_MICROFRAMES;
                else
                    Period = EndpointProperties->Period;

                break;
            }

            default:
            {
                DPRINT1("USBPORT_AllocateBandwidthUSB2: DeviceSpeed - %X!\n",
                        DeviceSpeed);

                DbgBreakPoint();

                Tt = &TtExtension->Tt;
                break;
            }
        }

        USB2_InitTtEndpoint(TtEndpoint,
                            TransferType,
                            Direction,
                            DeviceSpeed,
                            Period,
                            EndpointProperties->MaxPacketSize,
                            Tt);

        RebalanceListEntries = USB2_FRAMES - 2;

        Result = USB2_AllocateTimeForEndpoint(TtEndpoint,
                                              Rebalance,
                                              &RebalanceListEntries);

        if (Result)
        {
            Result = USB2_PromotePeriods(TtEndpoint,
                                         Rebalance,
                                         &RebalanceListEntries);
        }

        RebalanceListEntries = 0;

        for (ix = 0; Rebalance->RebalanceEndpoint[ix]; ix++)
        {
            RebalanceListEntries = ix + 1;
        }
    }
    else
    {
        RebalanceListEntries = 0;
        Result = FALSE;
    }

    DPRINT("USBPORT_AllocateBandwidthUSB2: RebalanceListEntries - %X, Result - %X\n",
           RebalanceListEntries,
           Result);

    for (ix = 0; ix < RebalanceListEntries; ix++)
    {
        RebalanceTtEndpoint = Rebalance->RebalanceEndpoint[ix];

        DPRINT("USBPORT_AllocateBandwidthUSB2: RebalanceTtEndpoint[%X] - %p, RebalanceTtEndpoint - %p, RebalanceLink - %p\n",
               ix,
               RebalanceTtEndpoint,
               &RebalanceTtEndpoint->Endpoint->RebalanceLink);

        InsertTailList(&RebalanceList,
                       &RebalanceTtEndpoint->Endpoint->RebalanceLink);
    }

    if (Rebalance)
        ExFreePoolWithTag(Rebalance, USB_PORT_TAG);

    if (Result)
    {
        SMask = USB2_GetSMASK(Endpoint->TtEndpoint);
        EndpointProperties->InterruptScheduleMask = SMask;

        CMask = USB2_GetCMASK(Endpoint->TtEndpoint);
        EndpointProperties->SplitCompletionMask = CMask;

        AllocedBusTime = TtEndpoint->CalcBusTime;

        EndpointBandwidth = USB2_MICROFRAMES * AllocedBusTime;
        EndpointProperties->UsbBandwidth = EndpointBandwidth;

        ActualPeriod = Endpoint->TtEndpoint->ActualPeriod;
        EndpointProperties->Period = ActualPeriod;

        ScheduleOffset = Endpoint->TtEndpoint->StartFrame;
        EndpointProperties->ScheduleOffset = ScheduleOffset;

        Factor = USB2_FRAMES / ActualPeriod;
        ASSERT(Factor);

        n = ScheduleOffset * Factor;

        if (TtExtension)
        {
            for (ix = 0; ix < Factor; ix++)
            {
                TtExtension->Bandwidth[n + ix] -= EndpointBandwidth;
            }
        }
        else
        {
            for (ix = 1; ix < Factor; ix++)
            {
                FdoExtension->Bandwidth[n + ix] -= EndpointBandwidth;
            }
        }

        USBPORT_DumpingEndpointProperties(EndpointProperties);
        USBPORT_DumpingTtEndpoint(Endpoint->TtEndpoint);

        if (AllocedBusTime >= (USB2_FS_MAX_PERIODIC_ALLOCATION / 2))
        {
            DPRINT1("USBPORT_AllocateBandwidthUSB2: AllocedBusTime >= 0.5 * MAX_ALLOCATION \n");
        }
    }

    USB2_Rebalance(FdoDevice, &RebalanceList);

    if (!TtExtension)
    {
        DPRINT("USBPORT_AllocateBandwidthUSB2: Result - %X\n", Result);
        return Result;
    }

    for (ix = 0; ix < USB2_FRAMES; ix++)
    {
        FdoExtension->Bandwidth[ix] += TtExtension->MaxBandwidth;
    }

    USBPORT_UpdateAllocatedBwTt(TtExtension);

    for (ix = 0; ix < USB2_FRAMES; ix++)
    {
        FdoExtension->Bandwidth[ix] -= TtExtension->MaxBandwidth;
    }

    DPRINT("USBPORT_AllocateBandwidthUSB2: Result - %X\n", Result);

    return Result;
}

VOID
NTAPI
USBPORT_FreeBandwidthUSB2(IN PDEVICE_OBJECT FdoDevice,
                          IN PUSBPORT_ENDPOINT Endpoint)
{
    DPRINT1("USBPORT_FreeBandwidthUSB2: UNIMPLEMENTED. FIXME. \n");
}

VOID
NTAPI
USB2_InitTT(IN PUSB2_HC_EXTENSION HcExtension,
            IN PUSB2_TT Tt)
{
    ULONG ix;
    ULONG jx;

    DPRINT("USB2_InitTT: HcExtension - %p, Tt - %p\n", HcExtension, Tt);

    Tt->HcExtension = HcExtension;
    Tt->DelayTime = 1;
    Tt->MaxTime = USB2_FS_MAX_PERIODIC_ALLOCATION;

    for (ix = 0; ix < USB2_FRAMES; ix++)
    {
        Tt->FrameBudget[ix].TimeUsed = USB2_MAX_MICROFRAMES;
        Tt->FrameBudget[ix].AltEndpoint = NULL;

        for (jx = 0; jx < USB2_MICROFRAMES; jx++)
        {
            Tt->TimeCS[ix][jx] = 0;
            Tt->NumStartSplits[ix][jx] = 0;
        }

        Tt->FrameBudget[ix].IsoEndpoint = &Tt->IsoEndpoint[ix];

        USB2_InitTtEndpoint(&Tt->IsoEndpoint[ix],
                            USBPORT_TRANSFER_TYPE_ISOCHRONOUS,
                            USBPORT_TRANSFER_DIRECTION_OUT,
                            UsbFullSpeed,
                            USB2_FRAMES,
                            0,
                            Tt);

        Tt->IsoEndpoint[ix].ActualPeriod = USB2_FRAMES;
        Tt->IsoEndpoint[ix].CalcBusTime = USB2_FS_SOF_TIME + USB2_HUB_DELAY;
        Tt->IsoEndpoint[ix].StartFrame = ix;
        Tt->IsoEndpoint[ix].StartMicroframe = 0xFF;

        Tt->FrameBudget[ix].IntEndpoint = &Tt->IntEndpoint[ix];

        USB2_InitTtEndpoint(&Tt->IntEndpoint[ix],
                            USBPORT_TRANSFER_TYPE_INTERRUPT,
                            USBPORT_TRANSFER_DIRECTION_OUT,
                            UsbFullSpeed,
                            USB2_FRAMES,
                            0,
                            Tt);

        Tt->IntEndpoint[ix].ActualPeriod = USB2_FRAMES;
        Tt->IntEndpoint[ix].CalcBusTime = USB2_FS_SOF_TIME + USB2_HUB_DELAY;
        Tt->IntEndpoint[ix].StartFrame = ix;
        Tt->IntEndpoint[ix].StartMicroframe = 0xFF;
    }
}

VOID
NTAPI
USB2_InitController(IN PUSB2_HC_EXTENSION HcExtension)
{
    ULONG ix;
    ULONG jx;

    DPRINT("USB2_InitController: HcExtension - %p\n", HcExtension);

    HcExtension->MaxHsBusAllocation = USB2_MAX_MICROFRAME_ALLOCATION;

    for (ix = 0; ix < USB2_FRAMES; ix++)
    {
        for (jx = 0; jx < USB2_MICROFRAMES; jx++)
        {
            HcExtension->TimeUsed[ix][jx] = 0;
        }
    }

    HcExtension->HcDelayTime = USB2_CONTROLLER_DELAY;

    USB2_InitTT(HcExtension, &HcExtension->HcTt);
}
