// Part of the DroidNet Driveable-Animation Contract — an additive layer added to a fork of
// reeltwo/WLogicEngine32 (Joymonkey, Rimim). Copyright (c) 2026 Travis Cook.
// SPDX-License-Identifier: LGPL-2.1-only
//
// src/contract/ContractSerial.h — the fork's Marcduino serial reader. It takes the place of the
// library's MarcduinoSerial<> on Serial1 (same constructor and setStream(), ZERO edits to
// lib/Reeltwo) and differs in three ways, all in contract_line.h:
//   * it drains every byte that has arrived, every pass, instead of one;
//   * a contract line ('!...') is handed to contractHandleLine() in this call, not queued on the
//     animation player, so a burst of contract lines all run;
//   * an over-long contract line is dropped rather than processed truncated.
// Every other line goes to Marcduino::processCommand() exactly as before, one per pass.
//
// INCLUDE ORDER (main.cpp): after contract/ContractLogics.h (contractHandleLine) and
// core/Marcduino.h.
#pragma once
#include "contract_line.h"

class ContractSerial : public AnimatedEvent
{
public:
    ContractSerial(AnimationPlayer& player) :
        fPlayer(player)
    {
    }

    void setStream(Stream* stream, Stream* outStream = nullptr)
    {
        fStream = stream;
        fOutStream = outStream;
    }

    virtual void animate() override
    {
        if (fStream == nullptr)
            return;
        // With a pass-through stream, take no more than it can accept without blocking: its
        // write() waits for room, and this is the task that renders the LEDs. A stream that
        // cannot say how much room it has (availableForWrite() == 0, Print's default) gets the
        // stock one byte per pass.
        uint8_t budget = CONTRACT_DRAIN_MAX;
        Stream* out = fOutStream;
        if (out != nullptr)
        {
            int room = out->availableForWrite();
            budget = (room <= 0) ? 1 : (room > CONTRACT_DRAIN_MAX ? CONTRACT_DRAIN_MAX : (uint8_t)room);
        }
        AnimationPlayer& player = fPlayer;
        contractDrain(fLine, *fStream, budget,
            [out](uint8_t b) { if (out != nullptr) out->write(&b, 1); },
            [](const char* afterBang) { contractHandleLine(afterBang); },
            [&player](const char* line) { Marcduino::processCommand(player, line); });
    }

private:
    Stream* fStream = nullptr;
    Stream* fOutStream = nullptr;
    AnimationPlayer& fPlayer;
    ContractLineBuf fLine;
};
