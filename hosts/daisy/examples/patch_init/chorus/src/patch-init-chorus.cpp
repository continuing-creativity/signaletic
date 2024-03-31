#include "daisy.h"
#include <libsignaletic.h>
#include "../../../../include/daisy-patch-sm-host.h"

#define SAMPLERATE 96000
#define DELAY_LINE_LENGTH SAMPLERATE
#define HEAP_SIZE 1024 * 384 // 384 KB
#define MAX_NUM_SIGNALS 32

float DSY_SDRAM_BSS leftDelayLineSamples[DELAY_LINE_LENGTH];
struct sig_Buffer leftDelayLineBuffer = {
    .length = DELAY_LINE_LENGTH,
    .samples = leftDelayLineSamples
};
struct sig_DelayLine leftDelayLine = {
    .buffer = &leftDelayLineBuffer,
    .writeIdx = 0
};

float DSY_SDRAM_BSS rightDelayLineSamples[DELAY_LINE_LENGTH];
struct sig_Buffer rightDelayLineBuffer = {
    .length = DELAY_LINE_LENGTH,
    .samples = rightDelayLineSamples
};
struct sig_DelayLine rightDelayLine = {
    .buffer = &rightDelayLineBuffer,
    .writeIdx = 0
};

uint8_t memory[HEAP_SIZE];
struct sig_AllocatorHeap heap = {
    .length = HEAP_SIZE,
    .memory = (void*) memory
};

struct sig_Allocator allocator = {
    .impl = &sig_TLSFAllocatorImpl,
    .heap = &heap
};

struct sig_dsp_Signal* listStorage[MAX_NUM_SIGNALS];
struct sig_List signals;
struct sig_dsp_SignalListEvaluator* evaluator;

daisy::patch_sm::DaisyPatchSM patchInit;
struct sig_daisy_Host* host;

struct sig_dsp_ConstantValue* blend;
struct sig_dsp_ConstantValue* feedforward;
struct sig_dsp_ConstantValue* feedback;
struct sig_dsp_ConstantValue* delayTime;
struct sig_dsp_ConstantValue* modulationSpeed;
struct sig_dsp_ConstantValue* modulationWidth;
struct sig_daisy_FilteredCVIn* blendKnob;
struct sig_daisy_FilteredCVIn* feedForwardKnob;
struct sig_daisy_FilteredCVIn* feedbackKnob;
struct sig_daisy_FilteredCVIn* delayTimeKnob;
struct sig_dsp_Chorus* leftChorus;
struct sig_dsp_Chorus* rightChorus;
struct sig_daisy_AudioIn* leftIn;
struct sig_daisy_AudioIn* rightIn;
struct sig_daisy_AudioOut* leftOut;
struct sig_daisy_AudioOut* rightOut;
struct sig_daisy_CVOut* modulatorLEDOut;

void buildSignalGraph(struct sig_SignalContext* context,
    struct sig_Status* status) {
    // White chorus
    // See Dattoro Part 2 pp. 775-6.
    blend = sig_dsp_ConstantValue_new(&allocator, context,
        0.7071f);
    feedforward = sig_dsp_ConstantValue_new(&allocator, context,
        1.0f); // Dattoro Part 2 p. 775
    feedback = sig_dsp_ConstantValue_new(&allocator, context,
        0.7071f); // Dattoro Part 2 p. 775
    delayTime = sig_dsp_ConstantValue_new(&allocator, context,
        0.00907029478458f); // ~9ms. Dattoro Part 2 p. 776.
    modulationSpeed = sig_dsp_ConstantValue_new(&allocator, context, 0.15f);
    modulationWidth = sig_dsp_ConstantValue_new(&allocator, context,
        0.007936507936508f); // ~8ms. Dattoro Part 2 p776.

    blendKnob = sig_daisy_FilteredCVIn_new(&allocator, context, host);
    sig_List_append(&signals, blendKnob, status);
    blendKnob->parameters.control = sig_daisy_PatchInit_KNOB_1;

    delayTimeKnob = sig_daisy_FilteredCVIn_new(&allocator, context, host);
    sig_List_append(&signals, delayTimeKnob, status);
    delayTimeKnob->parameters.control = sig_daisy_PatchInit_KNOB_2;

    feedForwardKnob = sig_daisy_FilteredCVIn_new(&allocator, context, host);
    sig_List_append(&signals, feedForwardKnob, status);
    feedForwardKnob->parameters.control = sig_daisy_PatchInit_KNOB_3;

    feedbackKnob = sig_daisy_FilteredCVIn_new(&allocator, context, host);
    sig_List_append(&signals, feedbackKnob, status);
    feedbackKnob->parameters.control = sig_daisy_PatchInit_KNOB_4;
    feedbackKnob->parameters.scale = 2.0f;
    feedbackKnob->parameters.offset = -1.0f;

    leftIn = sig_daisy_AudioIn_new(&allocator, context, host);
    sig_List_append(&signals, leftIn, status);
    leftIn->parameters.channel = 0;

    leftChorus = sig_dsp_Chorus_new(&allocator, context);
    sig_List_append(&signals, leftChorus, status);
    sig_DelayLine_init(&leftDelayLine);
    leftChorus->delayLine = &leftDelayLine;
    leftChorus->inputs.source = leftIn->outputs.main;
    leftChorus->inputs.delayTime = delayTimeKnob->outputs.main;
    leftChorus->inputs.blend = blendKnob->outputs.main;
    leftChorus->inputs.feedforwardGain = feedForwardKnob->outputs.main;
    leftChorus->inputs.feedbackGain = feedbackKnob->outputs.main;
    leftChorus->inputs.speed = modulationSpeed->outputs.main;
    leftChorus->inputs.width = modulationWidth->outputs.main;

    rightIn = sig_daisy_AudioIn_new(&allocator, context, host);
    sig_List_append(&signals, rightIn, status);
    rightIn->parameters.channel = 1;

    rightChorus = sig_dsp_Chorus_new(&allocator, context);
    sig_List_append(&signals, rightChorus, status);
    sig_DelayLine_init(&rightDelayLine);
    rightChorus->delayLine = &rightDelayLine;
    rightChorus->inputs.source = rightIn->outputs.main;
    rightChorus->inputs.delayTime = delayTimeKnob->outputs.main;
    rightChorus->inputs.blend = blendKnob->outputs.main;
    rightChorus->inputs.feedforwardGain = feedForwardKnob->outputs.main;
    rightChorus->inputs.feedbackGain = feedbackKnob->outputs.main;
    rightChorus->inputs.speed = modulationSpeed->outputs.main;
    rightChorus->inputs.width = modulationWidth->outputs.main;

    leftOut = sig_daisy_AudioOut_new(&allocator, context, host);
    sig_List_append(&signals, leftOut, status);
    leftOut->parameters.channel = sig_daisy_AUDIO_OUT_1;
    leftOut->inputs.source = leftChorus->outputs.main;

    rightOut = sig_daisy_AudioOut_new(&allocator, context, host);
    sig_List_append(&signals, rightOut, status);
    rightOut->parameters.channel = sig_daisy_AUDIO_OUT_2;
    rightOut->inputs.source = rightChorus->outputs.main;

    modulatorLEDOut = sig_daisy_CVOut_new(&allocator, context, host);
    sig_List_append(&signals, modulatorLEDOut, status);
    modulatorLEDOut->parameters.control = sig_daisy_PatchInit_LED;
    modulatorLEDOut->inputs.source = leftChorus->outputs.modulator;
    modulatorLEDOut->parameters.scale = 0.5f;
    modulatorLEDOut->parameters.offset = 0.5f;
}

int main(void) {
    allocator.impl->init(&allocator);

    struct sig_AudioSettings audioSettings = {
        .sampleRate = SAMPLERATE,
        .numChannels = 2,
        .blockSize = 1
    };

    struct sig_Status status;
    sig_Status_init(&status);
    sig_List_init(&signals, (void**) &listStorage, MAX_NUM_SIGNALS);

    evaluator = sig_dsp_SignalListEvaluator_new(&allocator, &signals);
    host = sig_daisy_PatchSMHost_new(&allocator,
        &audioSettings, &patchInit,
        (struct sig_dsp_SignalEvaluator*) evaluator);
    sig_daisy_Host_registerGlobalHost(host);

    struct sig_SignalContext* context = sig_SignalContext_new(&allocator,
        &audioSettings);
    buildSignalGraph(context, &status);
    host->impl->start(host);

    while (1) {

    }
}