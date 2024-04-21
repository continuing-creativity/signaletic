#pragma once

#include "signaletic-host.h"
#include "signaletic-daisy-host.h"
#include "sig-daisy-patch-sm.hpp"

using namespace sig::libdaisy;

enum {
    sig_host_KNOB_1 = 0,
    sig_host_KNOB_2,
    sig_host_KNOB_3,
    sig_host_KNOB_4,
    sig_host_KNOB_5,
    sig_host_KNOB_6
};

enum {
    sig_host_CV_IN_1 = 6,
    sig_host_CV_IN_2,
    sig_host_CV_IN_3,
    sig_host_CV_IN_4,
    sig_host_CV_IN_5,
    sig_host_CV_IN_6
};

enum {
    sig_host_CV_OUT_1 = 0
};

enum {
    sig_host_AUDIO_IN_1 = 0,
    sig_host_AUDIO_IN_2
};

enum {
    sig_host_AUDIO_OUT_1 = 0,
    sig_host_AUDIO_OUT_2
};

namespace lichen {
namespace medium {
    static const size_t NUM_ADC_CHANNELS = 12;

    // These pins are ordered based on the panel:
    // knobs first in labelled order, then CV jacks in labelled order.
    /*
    static ADCChannelSpec ADC_CHANNEL_SPECS[NUM_ADC_CHANNELS] = {
        // Freq Knob/POT_2_CV_7/Pin C8 - Unipolar
        {patchsm::PIN_CV_7, UNI_TO_BIPOLAR},
        // Reso Knob/CV_IN_5/Pin C6
        {patchsm::PIN_CV_5, UNI_TO_BIPOLAR},
        // Gain Knob/POT_1_CV_6/Pin C7
        {patchsm::PIN_CV_6, UNI_TO_BIPOLAR},
        // Knob four/POT_4_CV_9/Pin A2
        {patchsm::PIN_ADC_9, NO_NORMALIZATION},
        // Knob five/POT_5_CV_10/Pin A3
        {patchsm::PIN_ADC_10, NO_NORMALIZATION},

        // Shape CV/CV_IN_1/Pin C5
        {patchsm::PIN_CV_1, NO_NORMALIZATION},
        // Frequency CV/CV_IN_2/Pin C4
        {patchsm::PIN_CV_2, NO_NORMALIZATION},
        // Reso CV/CV_IN_3/Pin C3
        {patchsm::PIN_CV_3, NO_NORMALIZATION},
        // Gain CV/CV_IN_4/Pin C2
        {patchsm::PIN_CV_4, NO_NORMALIZATION},
        // Skew CV/POT_3_CV_8/Pin C9
        {patchsm::PIN_CV_5, NO_NORMALIZATION}
    };
    */
    static dsy_gpio_pin ADC_PINS[NUM_ADC_CHANNELS] = {
        patchsm::PIN_CV_5, // Knob one/POT_CV_1/Pin C8
        patchsm::PIN_CV_6, // Knob two/POT_CV_2/Pin C9
        patchsm::PIN_ADC_9, // Knob three/POT_CV_3/Pin A2
        patchsm::PIN_ADC_11, // Knob four/POT_CV_4/Pin A3
        patchsm::PIN_ADC_10, // Knob five/POT_CV_5/Pin D9
        patchsm::PIN_ADC_12, // Knob six/POT_CV_6/Pin D8

        patchsm::PIN_CV_1, // CV1 ("seven")/CV_IN_1/Pin C5
        patchsm::PIN_CV_2, // CV2 ("eight")/CV_IN_2Pin C4
        patchsm::PIN_CV_3, // CV3 ("nine")/CV_IN_3/Pin C3
        patchsm::PIN_CV_7, // CV6 ("ten")/CV_IN_6/Pin C7
        patchsm::PIN_CV_8, // CV5 ("eleven")/CV_IN_5/Pin C6
        patchsm::PIN_CV_4 // CV4 ("twelve")/CV_IN_4/Pin C2
    };

    static const size_t NUM_GATES = 1;

    static dsy_gpio_pin GATE_PINS[NUM_GATES] = {
        patchsm::PIN_B10
    };

    static const size_t NUM_TRISWITCHES = 1;
    static dsy_gpio_pin TRISWITCH_PINS[NUM_GATES][2] = {
        {
            patchsm::PIN_B7,
            patchsm::PIN_B8
        }
    };

    static const size_t NUM_BUTTONS = 1;
    static dsy_gpio_pin BUTTON_PINS[NUM_BUTTONS] = {
        patchsm::PIN_D1
    };

    static const size_t NUM_DAC_CHANNELS = 1;

    class MediumDevice {
        public:
            patchsm::PatchSMBoard board;
            ADCController<InvertedAnalogInput, NUM_ADC_CHANNELS> adcController;
            GateInput gates[NUM_GATES];
            InputBank<GateInput, NUM_GATES> gateBank;
            TriSwitch triSwitches[NUM_TRISWITCHES];
            InputBank<TriSwitch, NUM_TRISWITCHES> switchBank;
            Toggle buttons[NUM_BUTTONS];
            InputBank<Toggle, NUM_BUTTONS> buttonBank;
            AnalogOutput dacChannels[NUM_DAC_CHANNELS];
            OutputBank<AnalogOutput, NUM_DAC_CHANNELS> dacOutputBank;
            struct sig_host_HardwareInterface hardware;

            static void onEvaluateSignals(size_t size,
                struct sig_host_HardwareInterface* hardware) {
                MediumDevice* self = static_cast<MediumDevice*> (hardware->userData);
                self->Read();
            }

            static void afterEvaluateSignals(size_t size,
                struct sig_host_HardwareInterface* hardware) {
                MediumDevice* self = static_cast<MediumDevice*> (hardware->userData);
                self->Write();
            }

            void Init(struct sig_AudioSettings* audioSettings,
                struct sig_dsp_SignalEvaluator* evaluator) {
                board.Init(audioSettings->blockSize, audioSettings->sampleRate);
                // The DAC and ADC have to be initialized after the board.
                InitADCController();
                InitDAC();
                InitControls();

                hardware = {
                    .evaluator = evaluator,
                    .onEvaluateSignals = onEvaluateSignals,
                    .afterEvaluateSignals = afterEvaluateSignals,
                    .userData = this,
                    .numAudioInputChannels = 2,
                    .audioInputChannels = NULL, // Supplied by audio callback
                    .numAudioOutputChannels = 2,
                    .audioOutputChannels = NULL, // Supplied by audio callback
                    .numADCChannels = NUM_ADC_CHANNELS,
                    .adcChannels = adcController.channelBank.values,
                    .numDACChannels = NUM_DAC_CHANNELS,
                    .dacChannels = dacOutputBank.values,
                    .numGateInputs = NUM_GATES,
                    .gateInputs = gateBank.values,
                    .numGPIOOutputs = 0,
                    .gpioOutputs = NULL,
                    .numToggles = NUM_BUTTONS,
                    .toggles = buttonBank.values,
                    .numTriSwitches = NUM_TRISWITCHES,
                    .triSwitches = switchBank.values
                };
            }

            void InitADCController() {
                adcController.Init(&board.adc, ADC_PINS);
            }

            void InitDAC() {
                for (size_t i = 0; i < NUM_DAC_CHANNELS; i++) {
                    dacChannels[i].Init(board.dacOutputValues, i);
                }

                dacOutputBank.outputs = dacChannels;
            }

            void InitControls() {
                gates[0].Init(GATE_PINS[0]);
                gateBank.inputs = gates;
                triSwitches[0].Init(TRISWITCH_PINS[0]);
                switchBank.inputs = triSwitches;
                buttons[0].Init(BUTTON_PINS[0]);
                buttonBank.inputs = buttons;
            }

            void Start(daisy::AudioHandle::AudioCallback callback) {
                adcController.Start();
                board.StartDac();
                board.audio.Start(callback);
            }

            void Stop () {
                adcController.Stop();
                board.dac.Stop();
                board.audio.Stop();
            }

            inline void Read() {
                adcController.Read();
                gateBank.Read();
                switchBank.Read();
                buttonBank.Read();
            }

            inline void Write() {
                dacOutputBank.Write();
            }
    };
};
};