#pragma once

//#include <JuceHeader.h>

//==============================================================================
struct SineWaveSound   : public juce::SynthesiserSound
{
    SineWaveSound() {}

    bool appliesToNote    (int) override        { return true; }
    bool appliesToChannel (int) override        { return true; }
};

//==============================================================================
struct SineWaveVoice   : public juce::SynthesiserVoice
{
    
    SineWaveVoice(int edoSys)
    {
        sysEDO = edoSys;
    }

    bool canPlaySound (juce::SynthesiserSound* sound) override
    {
        return dynamic_cast<SineWaveSound*> (sound) != nullptr;
    }

    void startNote (int midiNoteNumber, float velocity,
                    juce::SynthesiserSound*, int /*currentPitchWheelPosition*/) override
    {
        currentAngle = 0.0;
        level = velocity * 0.15;
        tailOff = 0.0;
        //std::cout << "starting note: " + juce::String (midiNoteNumber) + " and hz is: " + juce::String (doMidiToHzConvert (midiNoteNumber, sysEDO)) << std::endl;
        auto cyclesPerSecond = doMidiToHzConvert (midiNoteNumber, sysEDO);
        auto cyclesPerSample = cyclesPerSecond / getSampleRate();

        angleDelta = cyclesPerSample * 2.0 * juce::MathConstants<double>::pi;
    }

    void stopNote (float /*velocity*/, bool allowTailOff) override
    {
        if (allowTailOff)
        {
            if (tailOff == 0.0)
                tailOff = 1.0;
        }
        else
        {
            clearCurrentNote();
            angleDelta = 0.0;
        }
    }

    void pitchWheelMoved (int) override      {}
    void controllerMoved (int, int) override {}

    void renderNextBlock (juce::AudioSampleBuffer& outputBuffer, int startSample, int numSamples) override
    {
        if (angleDelta != 0.0)
        {
            if (tailOff > 0.0) // [7]
            {
                while (--numSamples >= 0)
                {
                    auto currentSample = (float) (std::sin (currentAngle) * level * tailOff);

                    for (auto i = outputBuffer.getNumChannels(); --i >= 0;)
                        outputBuffer.addSample (i, startSample, currentSample);

                    currentAngle += angleDelta;
                    ++startSample;

                    tailOff *= 0.99; // [8]

                    if (tailOff <= 0.005)
                    {
                        clearCurrentNote(); // [9]

                        angleDelta = 0.0;
                        break;
                    }
                }
            }
            else
            {
                while (--numSamples >= 0) // [6]
                {
                    auto currentSample = (float) (std::sin (currentAngle) * level);

                    for (auto i = outputBuffer.getNumChannels(); --i >= 0;)
                        outputBuffer.addSample (i, startSample, currentSample);

                    currentAngle += angleDelta;
                    ++startSample;
                }
            }
        }
    }
    
    /*void changeEdo (int newEDO)
    {
        std::cout << "changing sysEDO in SineWaveVoice from: " + juce::String(sysEDO) + " to: " + juce::String(newEDO) << std::endl;

        sysEDO = newEDO;
        
    }
    */
    int getEDO ()
    {
        return sysEDO;
    }

private:
    double currentAngle = 0.0, angleDelta = 0.0, level = 0.0, tailOff = 0.0;
    float doMidiToHzConvert (int midiNoteNumber, int sysEDO)
    {
        //std::cout << "frequency calculating on sysEDO: " + juce::String (sysEDO) << std::endl;
        return pow(2, ((midiNoteNumber - 69.0) / sysEDO)) * 440.0;
    }
    int sysEDO;
    
    
};







//==============================================================================
class SynthAudioSource   : public juce::AudioSource
{

public:
    
    
    SynthAudioSource (juce::MidiKeyboardState& keyState, int EDOSys)
        : keyboardState (keyState)
    {
        sysEDO = EDOSys;

        for (auto i = 0; i < 10; ++i)  {             // [1]
            SineWaveVoice *sinWaveVoice = new SineWaveVoice(sysEDO);
            //sinWaveVoice->changeEdo(sysEDO);
            //std::cout << "EDO in voice: " + juce::String(sinWaveVoice->getEDO()) << std::endl;
            synth.addVoice (sinWaveVoice);
        }

        synth.addSound (new SineWaveSound());       // [2]
    }

    void setUsingSineWaveSound()
    {
        synth.clearSounds();
    }

    void prepareToPlay (int /*samplesPerBlockExpected*/, double sampleRate) override
    {
        synth.setCurrentPlaybackSampleRate (sampleRate); // [3]
        midiCollector.reset (sampleRate); // [10]
    }

    void releaseResources() override {}

    juce::MidiMessageCollector* getMidiCollector()
    {
        return &midiCollector;
    }
    
    void getNextAudioBlock (const juce::AudioSourceChannelInfo& bufferToFill) override
    {
        bufferToFill.clearActiveBufferRegion();

        juce::MidiBuffer incomingMidi;
        midiCollector.removeNextBlockOfMessages (incomingMidi, bufferToFill.numSamples); // [11]
        keyboardState.processNextMidiBuffer (incomingMidi, bufferToFill.startSample,
                                             bufferToFill.numSamples, true);       // [4]

        synth.renderNextBlock (*bufferToFill.buffer, incomingMidi,
                               bufferToFill.startSample, bufferToFill.numSamples); // [5]
    }
    
    void changeEDO (int newEDO)
    {
        std::cout << "Changing edo in synthaudiosource to: " + juce::String(newEDO) << std::endl;

        sysEDO = newEDO;
        
        synth.clearSounds();
        synth.clearVoices();
        for (auto i = 0; i < 10; ++i)  {             // [1]
            SineWaveVoice *sinWaveVoice = new SineWaveVoice(sysEDO);
            //sinWaveVoice->changeEdo(sysEDO);
            //std::cout << "EDO in voice: " + juce::String(sinWaveVoice->getEDO()) << std::endl;
            synth.addVoice (sinWaveVoice);
            std::cout << "in voice: " + juce::String (sinWaveVoice->getEDO()) << std::endl;
        }

        synth.addSound (new SineWaveSound());       // [2]
        
    }

private:
    juce::MidiKeyboardState& keyboardState;
    juce::Synthesiser synth;
    juce::MidiMessageCollector midiCollector;
    int sysEDO;
};


//==============================================================================
/*
    This component lives inside our window, and this is where you should put all
    your controls and content.
*/
class MainComponent  : public juce::AudioAppComponent,
                       private juce::MidiInputCallback,
                       private juce::MidiKeyboardStateListener,
                       private juce::Timer
 

{
public:
    //==============================================================================
    MainComponent()
      : keyboardComponent (keyboardState, juce::MidiKeyboardComponent::horizontalKeyboard),
        startTime (juce::Time::getMillisecondCounterHiRes() * 0.001),
        synthAudioSource  (keyboardState, 12)
    {
        setOpaque(true);
        
        addAndMakeVisible (midiInputList); //added but not yet populated
        setAudioChannels(0, 2);
        midiInputListLabel.setText ("MIDI Input:", juce::dontSendNotification);
        midiInputListLabel.attachToComponent (&midiInputList, true);//attaching to midiInputList using its reference
        
        addAndMakeVisible (EDOBox);
        EDOBoxLabel.setText ("System of EDO:", juce::dontSendNotification);
        EDOBoxLabel.attachToComponent (&EDOBox, true);
        
        auto midiInputs = juce::MidiInput::getAvailableDevices(); //grab midi devices using juce midi library
        
        juce::StringArray midiInputNames;//empty array of midi input names
        
        for (auto input : midiInputs)//polymorphic for loops are initialized w type auto
            midiInputNames.add (input.name);
        
        midiInputList.addItemList (midiInputNames, 1);
        midiInputList.onChange = [this] { setMidiInput (midiInputList.getSelectedItemIndex()); };
        /*juce::TextEditor::LengthAndCharacterRestriction *filters = new juce::TextEditor::LengthAndCharacterRestriction (10, "1234567890");
        EDOBox.setInputFilter (filters, false);*/
        EDOBox.onTextChange = [this] { changeEDOSys(EDOBox.getText().getIntValue()); };
        //find first enabled device in the list, use it by default
        for (auto input : midiInputs)
        {
            if (deviceManager.isMidiInputDeviceEnabled (input.identifier))
            {
                setMidiInput (midiInputs.indexOf (input));
                break;//ends the for loop if the condition is met -- no reason to continue
            }
        }
        
        //if no enabled devices found, just use first one
        if (midiInputList.getSelectedId() == 0)
            setMidiInput (0);
        
        
        addAndMakeVisible (keyboardComponent);
        keyboardState.addListener (this);
        
        
        addAndMakeVisible (midiMessagesBox);
        midiMessagesBox.setMultiLine (true);
        midiMessagesBox.setReturnKeyStartsNewLine (true);
        midiMessagesBox.setReadOnly (true);
        midiMessagesBox.setScrollbarsShown (true);
        midiMessagesBox.setCaretVisible (false);
        midiMessagesBox.setPopupMenuEnabled (true);
        midiMessagesBox.setColour (juce::TextEditor::backgroundColourId, juce::Colour (0x32ffffff));
        midiMessagesBox.setColour (juce::TextEditor::outlineColourId, juce::Colour (0x1c000000));
        midiMessagesBox.setColour (juce::TextEditor::shadowColourId, juce::Colour (0x16000000));


        setSize (600, 400);

    }
    ~MainComponent() override
    {
        keyboardState.removeListener (this);
        deviceManager.removeMidiInputDeviceCallback (juce::MidiInput::getAvailableDevices()[midiInputList.getSelectedItemIndex()].identifier, this);
        shutdownAudio();
   
    }

    //==============================================================================
    /*void prepareToPlay (int samplesPerBlockExpected, double sampleRate) override;
    void getNextAudioBlock (const juce::AudioSourceChannelInfo& bufferToFill) override;
    void releaseResources() override;
    */
    //==============================================================================
    void paint (juce::Graphics& g) override
    {
        // You can add your drawing code here!
        g.fillAll(juce::Colours::black);

      
    }
    void resized() override
    {
        auto area = getLocalBounds();

        EDOBox           .setBounds (area.removeFromTop (36).removeFromRight (getWidth() - 200).reduced (8));
        midiInputList    .setBounds (area.removeFromTop (36).removeFromRight (getWidth() - 200).reduced (8));
        keyboardComponent.setBounds (area.removeFromTop (80).reduced(8));
        midiMessagesBox  .setBounds (area.reduced (8));
     
    }
    
    
    void prepareToPlay (int samplesPerBlockExpected, double sampleRate) override
    {
        synthAudioSource.prepareToPlay (samplesPerBlockExpected, sampleRate);
    }

    void getNextAudioBlock (const juce::AudioSourceChannelInfo& bufferToFill) override
    {
        synthAudioSource.getNextAudioBlock (bufferToFill);
    }

    void releaseResources() override
    {
        synthAudioSource.releaseResources();
    }
    
    juce::MidiKeyboardState keyboardState;

private:
    //==============================================================================
    // Your private member variables go here...
    
    

    
    void timerCallback() override
    {
        keyboardComponent.grabKeyboardFocus();
        stopTimer();
    }

    static juce::String getMidiMessageDescription (const juce::MidiMessage& m)
      {
          if (m.isNoteOn())           return juce::String (m.getNoteNumber());
          if (m.isNoteOff())          return juce::String (m.getNoteNumber());
          if (m.isProgramChange())    return "Program change "   + juce::String (m.getProgramChangeNumber());
          if (m.isPitchWheel())       return "Pitch wheel "      + juce::String (m.getPitchWheelValue());
          if (m.isAftertouch())       return "After touch "      + juce::MidiMessage::getMidiNoteName (m.getNoteNumber(), true, true, 3) +  ": " + juce::String (m.getAfterTouchValue());
          if (m.isChannelPressure())  return "Channel pressure " + juce::String (m.getChannelPressureValue());
          if (m.isAllNotesOff())      return "All notes off";
          if (m.isAllSoundOff())      return "All sound off";
          if (m.isMetaEvent())        return "Meta event";

          if (m.isController())
          {
              juce::String name (juce::MidiMessage::getControllerName (m.getControllerNumber()));

              if (name.isEmpty())
                  name = "[" + juce::String (m.getControllerNumber()) + "]";

              return "Controller " + name + ": " + juce::String (m.getControllerValue());
          }

          return juce::String::toHexString (m.getRawData(), m.getRawDataSize());
      }
    /**start listening to a midi input device+enables it if necessary*/
    void setMidiInput (int index)
    {
        auto list = juce::MidiInput::getAvailableDevices();//use juce's library to grab available midi devices
        
        deviceManager.removeMidiInputDeviceCallback(list[lastInputIndex].identifier, this);
        //remove the midi device from the device manager by identifying it in the index of inputs
        deviceManager.removeMidiInputDeviceCallback (list[lastInputIndex].identifier,
                                                     synthAudioSource.getMidiCollector()); // [12]

        auto newInput = list[index];
        
        if (! deviceManager.isMidiInputDeviceEnabled (newInput.identifier))
            deviceManager.setMidiInputDeviceEnabled (newInput.identifier, true);
        
        deviceManager.addMidiInputDeviceCallback (newInput.identifier, synthAudioSource.getMidiCollector());
        deviceManager.addMidiInputDeviceCallback (newInput.identifier, this);
        midiInputList.setSelectedId (index + 1, juce::dontSendNotification);
        
        lastInputIndex = index;
        
    }

    void logMessage (const juce::String& m)
    {
        
        midiMessagesBox.moveCaretToEnd();
        midiMessagesBox.insertTextAtCaret (m + " Freq: " + juce::String (doMidiToHzConvert(m.substring(0,2).getIntValue(), EDOSys), 2, false ) + juce::newLine);
    }

    // These methods handle callbacks from the midi device + on-screen keyboard..
    void handleIncomingMidiMessage (juce::MidiInput* source, const juce::MidiMessage& message) override
    {
        //std::cout << "midiTST" << std::endl;
        const juce::ScopedValueSetter<bool> scopedInputFlag (isAddingFromMidiInput, true);
        keyboardState.processNextMidiEvent (message);
        postMessageToList (message, source->getName());
    }

    void handleNoteOn (juce::MidiKeyboardState*, int midiChannel, int midiNoteNumber, float velocity) override
    {
        if (! isAddingFromMidiInput)
        {
            auto m = juce::MidiMessage::noteOn (midiChannel, midiNoteNumber, velocity);
            m.setTimeStamp (juce::Time::getMillisecondCounterHiRes() * 0.001);
            postMessageToList (m, "On-Screen Keyboard");
        }
    }

    void handleNoteOff (juce::MidiKeyboardState*, int midiChannel, int midiNoteNumber, float /*velocity*/) override
    {
        if (! isAddingFromMidiInput)
        {
            auto m = juce::MidiMessage::noteOff (midiChannel, midiNoteNumber);
            m.setTimeStamp (juce::Time::getMillisecondCounterHiRes() * 0.001);
            postMessageToList (m, "On-Screen Keyboard");
        }
    }

    // This is used to dispach an incoming message to the message thread
    class IncomingMessageCallback   : public juce::CallbackMessage
    {
    public:
        IncomingMessageCallback (MainComponent* o, const juce::MidiMessage& m, const juce::String& s)
           : owner (o), message (m), source (s)
        {}

        void messageCallback() override
        {
            if (owner != nullptr)
                owner->addMessageToList (message, source);
        }

        Component::SafePointer<MainComponent> owner;
        juce::MidiMessage message;
        juce::String source;
    };

    void postMessageToList (const juce::MidiMessage& message, const juce::String& source)
    {
        (new IncomingMessageCallback (this, message, source))->post();
    }

    void addMessageToList (const juce::MidiMessage& message, const juce::String& source)
    {
        auto time = message.getTimeStamp() - startTime;

        auto hours   = ((int) (time / 3600.0)) % 24;
        auto minutes = ((int) (time / 60.0)) % 60;
        auto seconds = ((int) time) % 60;
        auto millis  = ((int) (time * 1000.0)) % 1000;

        auto timecode = juce::String::formatted ("%02d:%02d:%02d.%03d",
                                                 hours,
                                                 minutes,
                                                 seconds,
                                                 millis);

        auto description = getMidiMessageDescription (message);

        juce::String midiMessageString (/*timecode + "  -  " + */description + " (" + source + ")"); // [7]
        logMessage (midiMessageString);
    }
    
    void changeEDOSys (int newSys)
    {
        //std::cout << "changingEDOSYSinparent" << std::endl;
        EDOSys = newSys;
        synthAudioSource.changeEDO(newSys);
    }

    float doMidiToHzConvert (int midiNoteNumber, int sysEDO)
    {
        
        return pow(2, ((midiNoteNumber - 69.0) / sysEDO)) * 440.0;
    }
    
    //AudioDeviceManager helps us find which MIDI input devices are enabled
    juce::AudioDeviceManager deviceManager;
    //ComboBox will display list of midi inputs
    juce::ComboBox midiInputList;
    //
    juce::Label midiInputListLabel;
    //de-registers a previously selected midi input when a different input is selected
    int lastInputIndex = 0;
    //is midi data arriving from an outside source or the keyboard?
    bool isAddingFromMidiInput = false;
    
    //keeps track of which midi keys are currently held down
    
    //the on-screen keyboard component
    juce::MidiKeyboardComponent keyboardComponent;
    
    juce::Label EDOBoxLabel;
    juce::TextEditor EDOBox;
    juce::TextEditor midiMessagesBox;
    double startTime;
    
    SynthAudioSource synthAudioSource;

    //system of EDO we're in
    int EDOSys = 12;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainComponent)
};
