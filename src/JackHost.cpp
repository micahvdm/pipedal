// Copyright (c) 2022 Robin Davies
//
// Permission is hereby granted, free of charge, to any person obtaining a copy of
// this software and associated documentation files (the "Software"), to deal in
// the Software without restriction, including without limitation the rights to
// use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
// the Software, and to permit persons to whom the Software is furnished to do so,
// subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
// FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
// COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
// IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
// CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

#include "JackHost.hpp"

#include "Lv2Log.hpp"

#include "JackDriver.hpp"
#include "AlsaDriver.hpp"


using namespace pipedal;

#include "AudioConfig.hpp"


#include <string.h>
#include <stdio.h>
#include <mutex>
#include <thread>
#include <semaphore.h>
#include "VuUpdate.hpp"
#include "CpuGovernor.hpp"

#include "RingBuffer.hpp"
#include "RingBufferReader.hpp"

#include "PiPedalException.hpp"
#include "pthread.h"
#include "sched.h"
#include <cmath>
#include <chrono>
#include <fstream>
#include "Lv2EventBufferWriter.hpp"
#include "InheritPriorityMutex.hpp"

#ifdef __linux__
#include <sched.h>
#include <sys/syscall.h>
#include <unistd.h>
#endif

#define JACK_SESSION_CALLBACKS 0

#include "AdminClient.hpp"

const double VU_UPDATE_RATE_S = 1.0 / 30;
const double OVERRUN_GRACE_PERIOD_S = 15; 
using namespace pipedal;

const int MIDI_LV2_BUFFER_SIZE = 16 * 1024;


static void GetCpuFrequency(uint64_t*freqMin,uint64_t*freqMax)
{
    uint64_t fMax = 0;
    uint64_t fMin = UINT64_MAX;
    char deviceName[128];
    try {
    for (int i = 0; true; ++i)
    {
        
        snprintf(deviceName,sizeof(deviceName),"/sys/devices/system/cpu/cpu%d/cpufreq/scaling_cur_freq",i);
        std::ifstream f(deviceName);
        if (!f)
        {
            break;
        }
        uint64_t freq;
        f >> freq;
        if (!f) break;
        if (freq < fMin) fMin = freq;
        if (freq > fMax) fMax = freq;
    }
    } catch (const std::exception &)
    {

    }
    if (fMin == 0) fMax = 0;
    *freqMin = fMin;
    *freqMax = fMax;
}
static std::string GetGovernor()
{
    return pipedal::GetCpuGovernor();
}



class JackHostImpl : public JackHost, private AudioDriverHost
{
private:
    IHost *pHost = nullptr;

        class Uris
        {
        public:
            Uris(IHost *pHost)
            {
                atom_Blank = pHost->GetLv2Urid(LV2_ATOM__Blank);
                atom_Path = pHost->GetLv2Urid(LV2_ATOM__Path);
                atom_float = pHost->GetLv2Urid(LV2_ATOM__Float);
                atom_Double = pHost->GetLv2Urid(LV2_ATOM__Double);
                atom_Int = pHost->GetLv2Urid(LV2_ATOM__Int);
                atom_Long = pHost->GetLv2Urid(LV2_ATOM__Long);
                atom_Bool = pHost->GetLv2Urid(LV2_ATOM__Bool);
                atom_String = pHost->GetLv2Urid(LV2_ATOM__String);
                atom_Vector = pHost->GetLv2Urid(LV2_ATOM__Vector);
                atom_Object = pHost->GetLv2Urid(LV2_ATOM__Object);


                atom_Sequence = pHost->GetLv2Urid(LV2_ATOM__Sequence);
                atom_Chunk = pHost->GetLv2Urid(LV2_ATOM__Chunk);
                atom_URID = pHost->GetLv2Urid(LV2_ATOM__URID);
                atom_eventTransfer = pHost->GetLv2Urid(LV2_ATOM__eventTransfer);
                patch_Get = pHost->GetLv2Urid(LV2_PATCH__Get);
                patch_Set = pHost->GetLv2Urid(LV2_PATCH__Set);
                patch_Put = pHost->GetLv2Urid(LV2_PATCH__Put);
                patch_body = pHost->GetLv2Urid(LV2_PATCH__body);
                patch_subject = pHost->GetLv2Urid(LV2_PATCH__subject);
                patch_property = pHost->GetLv2Urid(LV2_PATCH__property);
                //patch_accept = pHost->GetLv2Urid(LV2_PATCH__accept);
                patch_value = pHost->GetLv2Urid(LV2_PATCH__value);
                unitsFrame = pHost->GetLv2Urid(LV2_UNITS__frame);

            }
            //LV2_URID patch_accept;

            LV2_URID unitsFrame;
            LV2_URID pluginUri;
            LV2_URID atom_Blank;
            LV2_URID atom_Bool;
            LV2_URID atom_float;
            LV2_URID atom_Double;
            LV2_URID atom_Int;
            LV2_URID atom_Long;
            LV2_URID atom_String;
            LV2_URID atom_Object;
            LV2_URID atom_Vector;
            LV2_URID atom_Path;
            LV2_URID atom_Sequence;
            LV2_URID atom_Chunk;
            LV2_URID atom_URID;
            LV2_URID atom_eventTransfer;
            LV2_URID midi_Event;
            LV2_URID patch_Get;
            LV2_URID patch_Set;
            LV2_URID patch_Put;
            LV2_URID patch_body;
            LV2_URID patch_subject;
            LV2_URID patch_property;
            LV2_URID patch_value;
            LV2_URID param_uiState;
        };

        Uris uris;



    AudioDriver*audioDriver = nullptr;

    inherit_priority_recursive_mutex mutex;
    int64_t overrunGracePeriodSamples = 0;

    IJackHostCallbacks *pNotifyCallbacks = nullptr;

    virtual void SetNotificationCallbacks(IJackHostCallbacks *pNotifyCallbacks)
    {
        this->pNotifyCallbacks = pNotifyCallbacks;
    }

    const size_t RING_BUFFER_SIZE = 64 * 1024;

    RingBuffer<true, false> inputRingBuffer;
    RingBuffer<false, true> outputRingBuffer;

    RingBufferWriter<true,false> x;

    RealtimeRingBufferReader realtimeReader;
    RealtimeRingBufferWriter realtimeWriter;
    HostRingBufferReader hostReader;
    HostRingBufferWriter hostWriter;

    JackChannelSelection channelSelection;
    bool active = false;

    std::shared_ptr<Lv2PedalBoard> currentPedalBoard;
    std::vector<std::shared_ptr<Lv2PedalBoard>> activePedalBoards; // pedalboards that have been sent to the audio queue.
    Lv2PedalBoard *realtimeActivePedalBoard = nullptr;

    std::vector<uint8_t *> midiLv2Buffers;

    uint32_t sampleRate = 0;
    uint64_t currentSample = 0;

    std::atomic<uint64_t> underruns = 0;
    std::atomic<std::chrono::system_clock::time_point> lastUnderrunTime =
        std::chrono::system_clock::from_time_t(0);


    std::string GetAtomObjectType(uint8_t*pData)
    {
        LV2_Atom_Object *pAtom = (LV2_Atom_Object*)pData;
        if (pAtom->atom.type != uris.atom_Object)
        {
            throw std::invalid_argument("Not an Lv2 Object");
        }
        return pHost->Lv2UriudToString(pAtom->body.otype);

    }

    void WriteAtom(json_writer &writer, LV2_Atom*pAtom);

    std::string AtomToJson(uint8_t*pData)
    {
        std::stringstream s;
        json_writer writer(s);
        LV2_Atom *pAtom = (LV2_Atom*)pData;

        WriteAtom(writer,pAtom);
        return s.str();
    }

    virtual void OnUnderrun()
    {
        ++this->underruns;
        this->lastUnderrunTime = std::chrono::system_clock ::now();
    }


    virtual void Close()
    {
        std::lock_guard guard(mutex);
        if (!isOpen)
            return;

        isOpen = false;
        StopReaderThread();

        if (realtimeMonitorPortSubscriptions != nullptr)
        {
            delete realtimeMonitorPortSubscriptions;
            realtimeMonitorPortSubscriptions = nullptr;
        }
        if (active)
        {
            audioDriver->Deactivate();

            active = false;
        }

        audioDriver->Close();

        // release any pdealboards owned by the process thread.
        this->activePedalBoards.resize(0);
        this->realtimeActivePedalBoard = nullptr;

        // clean up any realtime buffers that may have been lost in transit.
        // TODO: These should be lists, really. There may be multiple items in flight..
        if (realtimeVuBuffers != nullptr)
        {
            delete realtimeVuBuffers;
            realtimeVuBuffers = nullptr;
        }
        if (realtimeMonitorPortSubscriptions != nullptr)
        {
            delete realtimeMonitorPortSubscriptions;
            realtimeVuBuffers = nullptr;
        }
        this->inputRingBuffer.reset();
        this->outputRingBuffer.reset();


        for (size_t i = 0; i < midiLv2Buffers.size(); ++i)
        {
            delete[] midiLv2Buffers[i];
        }
        midiLv2Buffers.resize(0);

    }

    void ZeroBuffer(float *buffer, size_t nframes)
    {
        for (size_t i = 0; i < nframes; ++i)
        {
            buffer[i] = 0;
        }
    }
    Lv2EventBufferUrids eventBufferUrids;

    void ZeroOutputBuffers(size_t nframes)
    {
        for (size_t i = 0; i < audioDriver->OutputBufferCount(); ++i)
        {
            float *out = (float *)audioDriver->GetOutputBuffer(i, nframes);
            if (out)
            {
                ZeroBuffer(out, nframes);
            }
        }
    }
    RealtimeVuBuffers *realtimeVuBuffers = nullptr;
    size_t vuSamplesPerUpdate = 0;
    int64_t vuSamplesRemaining = 0;

    void freeRealtimeVuConfiguration()
    {
        if (this->realtimeVuBuffers != nullptr)
        {
            realtimeWriter.FreeVuSubscriptions(this->realtimeVuBuffers);

            this->realtimeVuBuffers = nullptr;
        }
    }

    RealtimeMonitorPortSubscriptions *realtimeMonitorPortSubscriptions = nullptr;

    void freeRealtimeMonitorPortSubscriptions()
    {
        if (this->realtimeMonitorPortSubscriptions != nullptr)
        {
            realtimeWriter.FreeMonitorPortSubscriptions(this->realtimeMonitorPortSubscriptions);

            this->realtimeMonitorPortSubscriptions = nullptr;
        }
    }

    void writeVu()
    {
        // throttling: we send one; but won't send another until the host thread
        // acknowledges receipt.

        if (!realtimeVuBuffers->waitingForAcknowledge)
        {
            auto pResult = realtimeVuBuffers->GetResult(currentSample);

            this->realtimeWriter.SendVuUpdate(pResult);
            realtimeVuBuffers->waitingForAcknowledge = true;
        }
    }

    void processMonitorPortSubscriptions(uint32_t nframes)
    {
        for (size_t i = 0; i < this->realtimeMonitorPortSubscriptions->subscriptions.size(); ++i)
        {
            auto &portSubscription = realtimeMonitorPortSubscriptions->subscriptions[i];

            portSubscription.samplesToNextCallback -= portSubscription.sampleRate;
            if (portSubscription.samplesToNextCallback < 0)
            {
                portSubscription.samplesToNextCallback += portSubscription.sampleRate;
                if (!portSubscription.waitingForAck)
                {
                    float value = realtimeActivePedalBoard->GetControlOutputValue(
                        portSubscription.instanceIndex,
                        portSubscription.portIndex);
                    if (value != portSubscription.lastValue)
                    {
                        portSubscription.waitingForAck = true;
                        portSubscription.lastValue = value;
                        this->realtimeWriter.SendMonitorPortUpdate(
                            portSubscription.callbackPtr,
                            portSubscription.subscriptionHandle,
                            value);
                    }
                }
            }
        }
    }

    RealtimeParameterRequest *pParameterRequests = nullptr;

    bool reEntered = false;
    void ProcessInputCommands()
    {
        if (reEntered)
        {
            throw PiPedalStateException("Rentry of process command.");
        }
        reEntered = true;

        while (true)
        {
            RingBufferCommand command;
            size_t space = realtimeReader.readSpace();
            if (space <= sizeof(RingBufferCommand)) // RingBufferCommand + at least 1 more byte for the body.
                break;
            if (!realtimeReader.read(&command))
            {
                break;
            }
            switch (command)
            {
            case RingBufferCommand::SetValue:
            {
                SetControlValueBody body;
                realtimeReader.readComplete(&body);
                this->realtimeActivePedalBoard->SetControlValue(body.effectIndex, body.controlIndex, body.value);
                break;
            }
            case RingBufferCommand::ParameterRequest:
            {
                RealtimeParameterRequest *pRequest = nullptr;
                realtimeReader.readComplete(&pRequest);

                // link to the list of parameter requests.
                pRequest->pNext = pParameterRequests;
                pParameterRequests = pRequest;
                break;
            }
            case RingBufferCommand::AckVuUpdate:
            {
                bool dummy;
                realtimeReader.readComplete(&dummy);
                if (this->realtimeVuBuffers)
                {
                    this->realtimeVuBuffers->waitingForAcknowledge = false;
                }

                break;
            }
            case RingBufferCommand::AckMonitorPortUpdate:
            {
                int64_t subscriptionHandle = 0;
                realtimeReader.readComplete(&subscriptionHandle);
                if (this->realtimeMonitorPortSubscriptions != nullptr)
                {
                    for (size_t i = 0; i < this->realtimeMonitorPortSubscriptions->subscriptions.size(); ++i)
                    {
                        if (this->realtimeMonitorPortSubscriptions->subscriptions[i].subscriptionHandle == subscriptionHandle)
                        {
                            this->realtimeMonitorPortSubscriptions->subscriptions[i].waitingForAck = false;
                        }
                    }
                }
                break;
            }
            case RingBufferCommand::SetMonitorPortSubscription:
            {
                RealtimeMonitorPortSubscriptions *pSubscriptions;
                realtimeReader.readComplete(&pSubscriptions);
                this->freeRealtimeMonitorPortSubscriptions();
                this->realtimeMonitorPortSubscriptions = pSubscriptions;
                break;
            }
            case RingBufferCommand::SetVuSubscriptions:
            {
                RealtimeVuBuffers *configuration;
                realtimeReader.readComplete(&configuration);
                this->freeRealtimeVuConfiguration();
                this->realtimeVuBuffers = configuration;
                if (this->realtimeVuBuffers != nullptr)
                {
                    this->realtimeVuBuffers->waitingForAcknowledge = false;
                }
                vuSamplesRemaining = vuSamplesPerUpdate;

                break;
            }
            case RingBufferCommand::SetBypass:
            {
                SetBypassBody body;
                realtimeReader.readComplete(&body);
                this->realtimeActivePedalBoard->SetBypass(body.effectIndex, body.enabled);
                break;
            }
            case RingBufferCommand::ReplaceEffect:
            {
                ReplaceEffectBody body;
                realtimeReader.readComplete(&body);

                auto oldValue = this->realtimeActivePedalBoard;
                this->realtimeActivePedalBoard = body.effect;

                realtimeWriter.EffectReplaced(oldValue);

                // invalidate the possibly no-good subscriptions. Model will update them shortly.
                freeRealtimeVuConfiguration();
                freeRealtimeMonitorPortSubscriptions();
                break;
            }
            default:
                throw PiPedalStateException("Unknown Ringbuffer command.");
            }
        }
        reEntered = false;
    }

    void OnMidiValueChanged(uint64_t instanceId, int controlIndex, float value)
    {
        realtimeWriter.MidiValueChanged(instanceId, controlIndex, value);
    }
    static void fnMidiValueChanged(void *data, uint64_t instanceId, int controlIndex, float value)
    {
        ((JackHostImpl *)data)->OnMidiValueChanged(instanceId, controlIndex, value);
    }
    void ProcessJackMidi()
    {
        Lv2EventBufferWriter eventBufferWriter(this->eventBufferUrids);

        size_t midiInputBufferCount = audioDriver->MidiInputBufferCount(); 

        audioDriver->FillMidiBuffers();
        
        for (size_t i = 0; i < midiInputBufferCount; ++i)
        {
            
            void *portBuffer = audioDriver->GetMidiInputBuffer(i,0);
            if (portBuffer)
            {
                uint8_t *lv2Buffer = this->midiLv2Buffers[i];
                size_t n = audioDriver->GetMidiInputEventCount(portBuffer);

                eventBufferWriter.Reset(lv2Buffer, MIDI_LV2_BUFFER_SIZE);
                auto iterator = eventBufferWriter.begin();

                for (size_t frame = 0; frame < n; ++frame)
                {
                    MidiEvent event;
                    if (audioDriver->GetMidiInputEvent(&event,portBuffer,frame)) 
                    {
                        eventBufferWriter.writeMidiEvent(iterator, 0, event.size, event.buffer);

                        this->realtimeActivePedalBoard->OnMidiMessage(event.size, event.buffer, this, fnMidiValueChanged);
                        if (listenForMidiEvent)
                        {
                            if (event.size >= 3)
                            {
                                uint8_t cmd = (uint8_t)(event.buffer[0] & 0xF0);
                                bool isNote = cmd == 0x90;
                                bool isControl = cmd == 0xB0;
                                if (isNote || isControl)
                                {
                                    realtimeWriter.OnMidiListen(isNote, event.buffer[1]);
                                }
                            }
                        }
                    }
                }
            }
        }
    }

#define RESET_XRUN_SAMPLES 22050ul // 1/2 a second-ish.

    std::mutex audioStoppedMutex;

    bool IsAudioActive()
    {
        std::lock_guard lock { audioStoppedMutex};
        return this->active;
    }
    virtual void OnAudioStopped()
    {
        std::lock_guard lock { audioStoppedMutex};
        this->active = false;
        Lv2Log::info("Audio stopped.");

    }

    virtual void OnProcess(size_t nframes)
    {
        try
        {
            float *in, *out;

            pParameterRequests = nullptr;

            ProcessInputCommands();

            bool processed = false;

            Lv2PedalBoard *pedalBoard = this->realtimeActivePedalBoard;
            if (pedalBoard != nullptr)
            {
                ProcessJackMidi();
                float *inputBuffers[4];
                float *outputBuffers[4];
                bool buffersValid = true;
                for (int i = 0; i < audioDriver->InputBufferCount(); ++i)
                {
                    float *input = (float *)audioDriver->GetInputBuffer(i, nframes);
                    if (input == nullptr)
                    {
                        buffersValid = false;
                        break;
                    }
                    inputBuffers[i] = input;
                }
                inputBuffers[audioDriver->InputBufferCount()] = nullptr;

                for (int i = 0; i < audioDriver->OutputBufferCount(); ++i)
                {
                    float *output =  audioDriver->GetOutputBuffer(i,nframes);
                    if (output == nullptr)
                    {
                        buffersValid = false;
                        break;
                    }
                    outputBuffers[i] = output;
                }
                outputBuffers[audioDriver->OutputBufferCount()] = nullptr;

                if (buffersValid)
                {
                    pedalBoard->ResetAtomBuffers();
                    pedalBoard->ProcessParameterRequests(pParameterRequests);

                    processed = pedalBoard->Run(inputBuffers, outputBuffers, (uint32_t)nframes,&realtimeWriter);
                    if (processed)
                    {
                        if (this->realtimeVuBuffers != nullptr)
                        {
                            pedalBoard->ComputeVus(this->realtimeVuBuffers, (uint32_t)nframes);

                            vuSamplesRemaining -= nframes;
                            if (vuSamplesRemaining <= 0)
                            {
                                writeVu();
                                vuSamplesRemaining += vuSamplesPerUpdate;
                            }
                        }
                        if (this->realtimeMonitorPortSubscriptions != nullptr)
                        {
                            processMonitorPortSubscriptions(nframes);
                        }
                    }
                    pedalBoard->GatherParameterRequests(pParameterRequests);
                }
            }

            // in = jack_port_get_buffer(input_port, nframes);
            // out = jack_port_get_buffer(output_port, nframes);
            // memcpy(out, in,
            //        sizeof(jack_default_audio_sample_t) * nframes);
            if (!processed)
            {
                ZeroOutputBuffers(nframes);
            }

            if (pParameterRequests != nullptr)
            {
                this->realtimeWriter.ParameterRequestComplete(pParameterRequests);
            }
            // provide a grace period for undderruns, while spinning up. (15 second-ish)
            if (currentSample <= this->overrunGracePeriodSamples && currentSample + nframes > this->overrunGracePeriodSamples)
            {
                this->underruns = 0;
            }
            this->currentSample += nframes;

        }
        catch (const std::exception &e)
        {
            Lv2Log::error("Fatal error while processing jack audio. (%s)", e.what());
            throw;
        }

    }
    
public:
    JackHostImpl(IHost *pHost)
        : inputRingBuffer(RING_BUFFER_SIZE),
          outputRingBuffer(RING_BUFFER_SIZE),
          realtimeReader(&this->inputRingBuffer),
          realtimeWriter(&this->outputRingBuffer),
          hostReader(&this->outputRingBuffer),
          hostWriter(&this->inputRingBuffer),
          eventBufferUrids(pHost),
          pHost(pHost),
          uris(pHost)
    {

        #if JACK_HOST 
        audioDriver = CreateJackDriver(this);
        #endif
        #if ALSA_HOST
        audioDriver = CreateAlsaDriver(this);
        #endif

    }
    virtual ~JackHostImpl()
    {
        Close();
        CleanRestartThreads(true);

        delete audioDriver;

    }

    virtual JackConfiguration GetServerConfiguration()
    {
        JackConfiguration result;

        result.JackInitialize();
        return result;
    }

    virtual uint32_t GetSampleRate()
    {
        return this->sampleRate;
    }

    void OnAudioComplete()
    {
        // there is actually no compelling circumstance in which this should ever happen.

        Lv2Log::error("Audio processing terminated unexpectedly.");
        realtimeWriter.AudioStopped();
    }
    std::vector<uint8_t> atomBuffer;

    bool terminateThread;
    void ThreadProc()
    {

#if defined(__WIN32)
        // bump thread prioriy two levels to
        // ensure that the service thread doesn't
        // get bogged down by UIwork. Doesn't have to be realtime, but it
        // MUST run at higher priority than UI threads.
        xxx; // TO DO.
#elif defined(__linux__)
        int min = sched_get_priority_min(SCHED_RR);
        int max = sched_get_priority_max(SCHED_RR);

        struct sched_param param;
        memset(&param, 0, sizeof(param));
        param.sched_priority = min;

        int result = sched_setscheduler(0, SCHED_RR, &param);
        if (result == 0)
        {
            Lv2Log::debug("Service thread priority successfully boosted.");
        }
#else
        xxx; // TODO!
#endif
        int underrunMessagesGiven = 0;
        try
        {

            struct timespec ts;
            // ever 30  seconds, timeout check for and log any overruns.
            int pollRateS = 30;
            if (clock_gettime(CLOCK_REALTIME, &ts) == -1)
            {
                Lv2Log::error("clock_gettime failed!");
                return;
            }
            ts.tv_sec += pollRateS;
            uint64_t lastUnderrunCount = this->underruns;

            while (true)
            {

                // wait for an event.
                // 0 -> ready. -1: timed out. -2: closing.

                int result = hostReader.wait(ts);
                if (result == -2)
                {
                    return;
                }
                else if (result == -1)
                {
                    // timeout.
                    ts.tv_sec += pollRateS;
                    uint64_t underruns = this->underruns;
                    if (underruns != lastUnderrunCount)
                    {
                        if (underrunMessagesGiven < 60) // limit how much log file clutter we generate.
                        {
                            Lv2Log::info("Jack - Underrun count: %lu", (unsigned long)underruns);
                            lastUnderrunCount = underruns;
                            ++underrunMessagesGiven;
                        }
                    }
                    clock_gettime(CLOCK_REALTIME, &ts);
                    ts.tv_sec += pollRateS;
                }
                else
                {
                    while (true)
                    {
                        size_t space = hostReader.readSpace();
                        if (space <= sizeof(RingBufferCommand))
                        {
                            break;
                        }
                        RingBufferCommand command;
                        if (hostReader.read(&command))
                        {
                            if (command == RingBufferCommand::OnMidiListen)
                            {
                                uint16_t msg;
                                hostReader.read(&msg);
                                if (this->pNotifyCallbacks)
                                {
                                    pNotifyCallbacks->OnNotifyMidiListen((msg & 0xFF00) != 0, (uint8_t)msg);
                                }
                            }
                            else if (command == RingBufferCommand::MidiValueChanged)
                            {
                                MidiValueChangedBody body;
                                hostReader.read(&body);

                                if (this->pNotifyCallbacks)
                                {
                                    this->pNotifyCallbacks->OnNotifyMidiValueChanged(body.instanceId, body.controlIndex, body.value);
                                }
                            }
                            else if (command == RingBufferCommand::ParameterRequestComplete)
                            {
                                RealtimeParameterRequest *pRequest = nullptr;
                                hostReader.read(&pRequest);

                                std::shared_ptr<Lv2PedalBoard> currentpedalBoard;

                                {
                                    std::lock_guard guard(mutex);
                                    currentPedalBoard = this->currentPedalBoard;
                                }

                                while (pRequest != nullptr)
                                {
                                    auto pNext = pRequest->pNext;
                                    if (pRequest->errorMessage == nullptr && pRequest->responseLength != 0)
                                    {
                                        IEffect *pEffect = currentPedalBoard->GetEffect(pRequest->instanceId);
                                        if (pEffect == nullptr)
                                        {
                                            pRequest->errorMessage = "Effect no longer available.";
                                        }
                                        else
                                        {
                                            pRequest->jsonResponse = AtomToJson(pRequest->response);
                                        }
                                    }
                                    pRequest->onJackRequestComplete(pRequest);
                                    pRequest = pNext;
                                }
                            }
                            else if (command == RingBufferCommand::SendMonitorPortUpdate)
                            {
                                MonitorPortUpdate body;
                                hostReader.read(&body);

                                if (this->pNotifyCallbacks != nullptr)
                                {
                                    this->pNotifyCallbacks->OnNotifyMonitorPort(body);
                                }
                                this->hostWriter.AckMonitorPortUpdate(body.subscriptionHandle); // please sir, can I have some more?
                            }
                            else if (command == RingBufferCommand::SendVuUpdate)
                            {
                                const std::vector<VuUpdate> *updates = nullptr;
                                hostReader.read(&updates);

                                if (this->pNotifyCallbacks)
                                {
                                    this->pNotifyCallbacks->OnNotifyVusSubscription(*updates);
                                }
                                this->hostWriter.AckVuUpdate(); // please sir, can I have some more?
                            } else if (command == RingBufferCommand::AtomOutput)
                            {
                                uint64_t instanceId;
                                hostReader.read(&instanceId);
                                size_t extraBytes;
                                hostReader.read(&extraBytes);
                                if (atomBuffer.size() < extraBytes)
                                {
                                    atomBuffer.resize(extraBytes);
                                }
                                hostReader.read(extraBytes,&(atomBuffer[0]));

                        
                                IEffect *pEffect = currentPedalBoard->GetEffect(instanceId);
                                if (pEffect != nullptr &&this->pNotifyCallbacks && listenForAtomOutput)
                                {
                                    std::string atomType = GetAtomObjectType(&atomBuffer[0]);
                                    auto json  = AtomToJson(&(atomBuffer[0]));
                                    this->pNotifyCallbacks->OnNotifyAtomOutput(instanceId,atomType,json);
                                }
                            }
                            else if (command == RingBufferCommand::FreeVuSubscriptions)
                            {
                                RealtimeVuBuffers *config;
                                hostReader.read(&config);
                                delete config;
                            }
                            else if (command == RingBufferCommand::FreeMonitorPortSubscription)
                            {
                                RealtimeMonitorPortSubscriptions *pSubscriptions;
                                hostReader.read(&pSubscriptions);
                                delete pSubscriptions;
                            }
                            else if (command == RingBufferCommand::EffectReplaced)
                            {
                                EffectReplacedBody body;
                                hostReader.read(&body);
                                OnActivePedalBoardReleased(body.oldEffect);
                            }
                            else if (command == RingBufferCommand::AudioStopped)
                            {
                                AudioStoppedBody body;
                                hostReader.read(&body);
                                OnAudioComplete();
                                return;
                            }
                            else
                            {
                                throw PiPedalStateException("Unrecognized command received from audio thread.");
                            }
                        }
                    }
                }
            }
        }
        catch (const std::exception &e)
        {
            Lv2Log::error("Realtime response thread terminated abnormally. (%s)", e.what());
        }
    }

    std::thread *readerThread = nullptr;

    void StopReaderThread()
    {
        if (readerThread != nullptr)
        {
            this->terminateThread = true;
            this->outputRingBuffer.close();

            readerThread->join();
            delete readerThread;
            readerThread = nullptr;
        }
    }
    void StartReaderThread()
    {
        terminateThread = false;
        auto f = [this]()
        {
            this->ThreadProc();
        };

        this->readerThread = new std::thread(f);
    }

    bool isOpen = false;

    virtual bool IsOpen() const
    {
        return isOpen;
    }

    uint8_t *AllocateRealtimeBuffer(size_t size)
    {
        uint8_t *result = new uint8_t[size];
        // populate pages if required. We should have mlocked-ed alread, so they will stay.
        for (size_t i = 0; i < size; i += 1024)
        {
            result[i] = 0;
        }
        return result;
    }


    virtual void Open(const JackServerSettings &jackServerSettings,const JackChannelSelection &channelSelection)
    {

        std::lock_guard guard(mutex);
        if (channelSelection.GetInputAudioPorts().size() == 0
        || channelSelection.GetOutputAudioPorts().size() == 0)
        {
            return;
        }

        this->currentSample = 0;
        this->underruns = 0;

        if (isOpen)
        {
            throw PiPedalStateException("Already open.");
        }
        isOpen = true;

        this->inputRingBuffer.reset();
        this->outputRingBuffer.reset();

        this->channelSelection = channelSelection;

        StartReaderThread();

        try {

            audioDriver->Open(jackServerSettings,this->channelSelection);

            this->sampleRate = audioDriver->GetSampleRate();

            this->overrunGracePeriodSamples = (uint64_t)(((uint64_t)this->sampleRate)*OVERRUN_GRACE_PERIOD_S);
            this->vuSamplesPerUpdate = (size_t)(sampleRate * VU_UPDATE_RATE_S);


            midiLv2Buffers.resize(audioDriver->MidiInputBufferCount());
            for (size_t i = 0; i < audioDriver->MidiInputBufferCount(); ++i)
            {
                midiLv2Buffers[i] = AllocateRealtimeBuffer(MIDI_LV2_BUFFER_SIZE);
            }


            active = true;
            audioDriver->Activate();
            Lv2Log::info("Audio started.");

        }
        catch (const std::exception &e)
        {
            Lv2Log::error(SS("Failed to start audio. " << e.what()));
            Close();
            active = false;
            throw;
        }
    }

    void OnActivePedalBoardReleased(Lv2PedalBoard *pPedalBoard)
    {
        if (pPedalBoard)
        {
            pPedalBoard->Deactivate();
            std::lock_guard guard(mutex);

            for (auto it = activePedalBoards.begin(); it != activePedalBoards.end(); ++it)
            {
                if ((*it).get() == pPedalBoard)
                {
                    // erase it, relinquishing shared_ptr ownership, usually deleting the object.
                    activePedalBoards.erase(it);
                    return;
                }
            }
        }
    }

    virtual void SetPedalBoard(const std::shared_ptr<Lv2PedalBoard> &pedalBoard)
    {
        std::lock_guard guard(mutex);

        this->currentPedalBoard = pedalBoard;
        if (active)
        {
            pedalBoard->Activate();
            this->activePedalBoards.push_back(pedalBoard);
            hostWriter.ReplaceEffect(pedalBoard.get());
        }
    }

    virtual void SetBypass(uint64_t instanceId, bool enabled)
    {
        std::lock_guard guard(mutex);
        if (active && this->currentPedalBoard)
        {
            // use indices not instance ids, so we can just do a straight array index on the audio thread.
            auto index = currentPedalBoard->GetIndexOfInstanceId(instanceId);
            if (index >= 0)
            {
                hostWriter.SetBypass((uint32_t)index, enabled);
            }
        }
    }

    virtual void SetPluginPreset(uint64_t instanceId, const std::vector<ControlValue> &values)
    {
        std::lock_guard guard(mutex);
        if (active && this->currentPedalBoard)
        {
            auto effectIndex = currentPedalBoard->GetIndexOfInstanceId(instanceId);
            if (effectIndex != -1)
            {
                for (size_t i = 0; i < values.size(); ++i)
                {
                    const ControlValue &value = values[i];
                    int controlIndex = this->currentPedalBoard->GetControlIndex(instanceId, value.key());
                    if (controlIndex != -1 && effectIndex != -1)
                    {
                        hostWriter.SetControlValue(effectIndex, controlIndex, value.value());
                    }
                }
            }
        }
    }

    void SetControlValue(uint64_t  instanceId, const std::string &symbol, float value)
    {
        std::lock_guard guard(mutex);
        if (active && this->currentPedalBoard)
        {
            // use indices not instance ids, so we can just do a straight array index on the audio thread.
            int controlIndex = this->currentPedalBoard->GetControlIndex(instanceId, symbol);
            auto effectIndex = currentPedalBoard->GetIndexOfInstanceId(instanceId);

            if (controlIndex != -1 && effectIndex != -1)
            {
                hostWriter.SetControlValue(effectIndex, controlIndex, value);
            }
        }
    }

    virtual void SetVuSubscriptions(const std::vector<int64_t> &instanceIds)
    {
        std::lock_guard guard(mutex);

        if (active && this->currentPedalBoard)
        {

            if (instanceIds.size() == 0)
            {
                this->hostWriter.SetVuSubscriptions(nullptr);
            }
            else
            {
                RealtimeVuBuffers *vuConfig = new RealtimeVuBuffers();

                for (size_t i = 0; i < instanceIds.size(); ++i)
                {
                    int64_t instanceId = instanceIds[i];
                    auto effect = this->currentPedalBoard->GetEffect(instanceId);
                    if (!effect)
                    {
                        throw PiPedalStateException("Effect not found.");
                    }

                    int index = this->currentPedalBoard->GetIndexOfInstanceId(instanceIds[i]);
                    vuConfig->enabledIndexes.push_back(index);
                    VuUpdate v;
                    v.instanceId_ = instanceId;
                    // Display mono VUs if a stereo device is being fed identical L/R inputs.
                    v.isStereoInput_ = effect->GetNumberOfInputAudioPorts() != 1 && effect->GetAudioInputBuffer(0) != effect->GetAudioInputBuffer(1);
                    v.isStereoOutput_ = effect->GetNumberOfOutputAudioPorts() != 1;

                    vuConfig->vuUpdateWorkingData.push_back(v);
                    vuConfig->vuUpdateResponseData.push_back(v);
                }

                this->hostWriter.SetVuSubscriptions(vuConfig);
            }
        }
    }

    RealtimeMonitorPortSubscription MakeRealtimeSubscription(const MonitorPortSubscription &subscription)
    {
        RealtimeMonitorPortSubscription result;
        result.subscriptionHandle = subscription.subscriptionHandle;
        result.instanceIndex = this->currentPedalBoard->GetIndexOfInstanceId(subscription.instanceid);
        IEffect *pEffect = this->currentPedalBoard->GetEffect(subscription.instanceid);

        result.portIndex = pEffect->GetControlIndex(subscription.key);
        result.sampleRate = (int)(this->GetSampleRate() * subscription.updateInterval);
        result.samplesToNextCallback = result.sampleRate;
        PortMonitorCallback *ptr = new PortMonitorCallback(subscription.onUpdate);
        result.callbackPtr = ptr;
        return result;
    }
    virtual void SetMonitorPortSubscriptions(const std::vector<MonitorPortSubscription> &subscriptions)
    {
        if (!active)
            return;
        if (this->currentPedalBoard == nullptr)
            return;
        if (subscriptions.size() == 0)
        {
            this->hostWriter.SetMonitorPortSubscriptions(nullptr);
        }
        else
        {
            RealtimeMonitorPortSubscriptions *pSubscriptions = new RealtimeMonitorPortSubscriptions();

            for (size_t i = 0; i < subscriptions.size(); ++i)
            {
                if (this->currentPedalBoard->GetEffect(subscriptions[i].instanceid) != nullptr)
                {
                    pSubscriptions->subscriptions.push_back(
                        MakeRealtimeSubscription(subscriptions[i]));
                }
            }
            this->hostWriter.SetMonitorPortSubscriptions(pSubscriptions);
        }
    }

private:
    class RestartThread
    {
        JackHostImpl *this_;
        JackServerSettings jackServerSettings;
        std::function<void(bool success, const std::string &errorMessage)> onComplete;
        std::atomic<bool> isComplete = false;
        std::thread *pThread = nullptr;

    public:
        RestartThread(
            JackHostImpl *host,
            const JackServerSettings &jackServerSettings_,
            std::function<void(bool success, const std::string &errorMessage)> onComplete_)
            : this_(host),
              jackServerSettings(jackServerSettings_),
              onComplete(onComplete_)
        {
        }
        ~RestartThread()
        {
            pThread->join();
            delete pThread;
        }
        bool IsComplete() const { return isComplete; }

        void ThreadProc()
        {
            this_->restarting = true;
            // this_->Close(); (JackServerConfiguration now does a service restart.)
            try {
                AdminClient client;

                client.SetJackServerConfiguration(jackServerSettings);


                //this_->Open(this_->channelSelection);
                this_->restarting = false;
                onComplete(true, "");
                isComplete = true;
            } catch (const std::exception &e)
            {
                onComplete(false,e.what());
                this_->restarting = false;
                isComplete = true;
            }
        }
        static void ThreadProc_(RestartThread *this_)
        {
            this_->ThreadProc();
        }
        void Run()
        {
            pThread = new std::thread(ThreadProc_, this);
        }
    };

    bool restarting = false;
    std::vector<RestartThread *> restartThreads;

public:
    inherit_priority_recursive_mutex restart_mutex;
    virtual void UpdateServerConfiguration(const JackServerSettings &jackServerSettings,
                                           std::function<void(bool success, const std::string &errorMessage)> onComplete)
    {
        std::lock_guard guard(restart_mutex);
        RestartThread *pShutdown = new RestartThread(this, jackServerSettings, onComplete);
        restartThreads.push_back(pShutdown);
        pShutdown->Run();
    }
    void CleanRestartThreads(bool final)
    {
        std::lock_guard guard(restart_mutex);
        for (size_t i = 0; i < restartThreads.size(); ++i)
        {
            if (final)
            {
                delete restartThreads[i];
            }
            else
            {
                if (restartThreads[i]->IsComplete())
                {
                    delete restartThreads[i];
                    restartThreads.erase(restartThreads.begin() + i);
                    --i;
                }
            }
        }
        if (final)
        {
            restartThreads.clear();
        }
    }

    virtual void getRealtimeParameter(RealtimeParameterRequest *pParameterRequest)
    {
        if (!active)
        {
            pParameterRequest->errorMessage = "Not active.";
            pParameterRequest->onJackRequestComplete(pParameterRequest);
            return;
        }
        this->hostWriter.ParameterRequest(pParameterRequest);
    }

    static int32_t GetRaspberryPiTemperature()
    {
        try
        {
            std::ifstream f("/sys/class/thermal/thermal_zone0/temp");
            int32_t temp;
            f >> temp;
            return temp;
        }
        catch (std::exception &)
        {
            return -1000000;
        }
    }

    virtual JackHostStatus getJackStatus()
    {
        CleanRestartThreads(false);
        using namespace std::chrono;
        using std::chrono::duration_cast;
        using std::chrono::milliseconds;

        std::lock_guard guard(mutex);

        JackHostStatus result;
        result.underruns_ = this->underruns;
        auto dt = duration_cast<milliseconds>(std::chrono::system_clock ::now().time_since_epoch()).count() - duration_cast<milliseconds>(this->lastUnderrunTime.load().time_since_epoch()).count();

        result.msSinceLastUnderrun_ = (uint64_t)dt;

        result.temperaturemC_ = GetRaspberryPiTemperature();

        result.active_ = IsAudioActive();
        result.restarting_ = this->restarting;

        if (this->audioDriver != nullptr)
        {
            result.cpuUsage_ = audioDriver->CpuUse();
        }
        GetCpuFrequency(&result.cpuFreqMax_,&result.cpuFreqMin_);
        result.governor_ = GetGovernor();

        return result;
    }
    volatile bool listenForMidiEvent = false;
    volatile bool listenForAtomOutput = false;

    virtual void SetListenForMidiEvent(bool listen)
    {
        this->listenForMidiEvent = listen;
    }
    virtual void SetListenForAtomOutput(bool listen)
    {
        this->listenForAtomOutput = listen;
    }
};

static std::string UriToFieldName(const std::string&uri){
    int pos;
    for (pos = uri.length(); pos >= 0; --pos)
    {
        char c = uri[pos];
        if (c == '#' || c == '/' || c == ':')
        {
            break;
        }
    }
    return uri.substr(pos+1);
}

void JackHostImpl::WriteAtom(json_writer &writer, LV2_Atom*pAtom)
{
    if (pAtom->type == uris.atom_Blank)
    {
        writer.write_raw("null");
    }
    else if (pAtom->type == uris.atom_float)
    {
        writer.write(
            ((LV2_Atom_Float*)pAtom)->body
        );

    } else if (pAtom->type == uris.atom_Int)
    {
        writer.write(
            ((LV2_Atom_Int*)pAtom)->body
        );
    } else if (pAtom->type == uris.atom_Long)
    {
        writer.write(
            ((LV2_Atom_Long*)pAtom)->body
        );
    } else if (pAtom->type == uris.atom_Double)
    {
        writer.write(
            ((LV2_Atom_Double*)pAtom)->body
        );
    } else if (pAtom->type == uris.atom_Bool)
    {
        writer.write(
            ((LV2_Atom_Bool*)pAtom)->body
        );
    } else if (pAtom->type == uris.atom_String)
    {
        const char *p = (((const char*) pAtom) + sizeof(LV2_Atom_String));
        writer.write(
            p
        );
    } else if (pAtom->type == uris.atom_Vector)
    {
        LV2_Atom_Vector *pVector = (LV2_Atom_Vector*)pAtom;
        writer.start_array();
        {
            size_t n = (pAtom->size-sizeof(pVector->body))/pVector->body.child_size;
            char *pItems = ((char*)pAtom) + sizeof(LV2_Atom_Vector);
            if (pVector->body.child_type == uris.atom_float)
            {
                float *p = (float*)pItems;
                for (size_t i = 0; i < n; ++i)
                {
                    if (i != 0) writer.write_raw(",");
                    writer.write(*p++);
                }
            } else if (pVector->body.child_type == uris.atom_Int)
            {
                int32_t *p = (int32_t*)pItems;
                for (size_t i = 0; i < n; ++i)
                {
                    if (i != 0) writer.write_raw(",");
                    writer.write(*p++);
                }
            } else if (pVector->body.child_type == uris.atom_Long)
            {
                int64_t *p = (int64_t*)pItems;
                for (size_t i = 0; i < n; ++i)
                {
                    if (i != 0) writer.write_raw(",");
                    writer.write(*p++);
                }
            } else if (pVector->body.child_type == uris.atom_Double)
            {
                double *p = (double*)pItems;
                for (size_t i = 0; i < n; ++i)
                {
                    if (i != 0) writer.write_raw(",");
                    writer.write(*p++);
                }
            } else if (pVector->body.child_type == uris.atom_Bool)
            {
                bool *p = (bool*)pItems;
                for (size_t i = 0; i < n; ++i)
                {
                    if (i != 0) writer.write_raw(",");
                    writer.write(*p++);
                }
            }
        }
        writer.end_array();
    } else if (pAtom->type == uris.atom_Object)
    {
        writer.start_object();

        const LV2_Atom_Object *obj = (const LV2_Atom_Object *)pAtom;
        bool firstMember = true;
        if (obj->body.id != 0)
        {
            std::string id = pHost->Lv2UriudToString(obj->body.id);
            writer.write_member("id",id.c_str());
            firstMember = false;
        }

        if (obj->body.otype != 0)
        {
            std::string type =  pHost->Lv2UriudToString(obj->body.otype);        
            writer.write_member("lv2Type",type.c_str());
            if (!firstMember)
            {
                writer.write_raw(",");
            }
            firstMember = false;
        }

        LV2_ATOM_OBJECT_FOREACH (obj, prop) {
            if (!firstMember) {
                writer.write_raw(",");
            }
            firstMember = false;
            std::string key = pHost->Lv2UriudToString(prop->key);
            key = UriToFieldName(key);
            writer.write(key);
            writer.write_raw(": ");
            LV2_Atom *value = &(prop->value);
            WriteAtom(writer,value);
        }
        writer.end_object();
        
    }

}

JackHost *JackHost::CreateInstance(IHost *pHost)
{
    return new JackHostImpl(pHost);
}

JSON_MAP_BEGIN(JackHostStatus)
JSON_MAP_REFERENCE(JackHostStatus, active)
JSON_MAP_REFERENCE(JackHostStatus, errorMessage)
JSON_MAP_REFERENCE(JackHostStatus, restarting)
JSON_MAP_REFERENCE(JackHostStatus, underruns)
JSON_MAP_REFERENCE(JackHostStatus, cpuUsage)
JSON_MAP_REFERENCE(JackHostStatus, msSinceLastUnderrun)
JSON_MAP_REFERENCE(JackHostStatus, temperaturemC)
JSON_MAP_REFERENCE(JackHostStatus, cpuFreqMin)
JSON_MAP_REFERENCE(JackHostStatus, cpuFreqMax)
JSON_MAP_REFERENCE(JackHostStatus, governor)
JSON_MAP_END()
