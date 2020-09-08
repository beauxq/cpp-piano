#include <SFML/Graphics.hpp>
#include <SFML/Audio.hpp>

#include <cmath>
#include <unordered_map>
#include <vector>
#include <array>

// #include <iostream>  // debugging

constexpr double pi = 3.14159265358979323846;

struct Piano
{
    const int sampleRate = 44100;
    const int transpose;

    // 40 is the number of notes made in makeNotes
    // using array to make sure they don't move
    std::array<sf::SoundBuffer, 40> noteBuffers;

    std::unordered_map<sf::Keyboard::Key, sf::Sound> notes;

    sf::RenderWindow window;

    Piano(const int transpose=0) : transpose(transpose)
    {
        // std::cout << "in ctor, before makeNotes" << std::endl;
        makeNotes();
        /* debugging
        std::cout << "made notes, size: " << notes.size() << std::endl;
        std::cout << "buffers size: " << noteBuffers.size() << std::endl;
        std::cout << "buffer sample count: " << noteBuffers[0].getSampleCount() << std::endl;
        std::cout << "buffer contents: " << noteBuffers[0].getSamples()[1] << std::endl;
        std::cout << "buffer address from array: " << &(noteBuffers[0]) << std::endl;
        std::cout << "buffer address from note: " << notes[sf::Keyboard::Z].getBuffer() << std::endl;
        std::cout << "buffer sample count from note: " << notes[sf::Keyboard::Z].getBuffer()->getSampleCount() << std::endl;
        */
    }

    void run()
    {
        window.create(sf::VideoMode(150, 50), "Piano");
        window.setFramerateLimit(180);
        window.setKeyRepeatEnabled(false);

        while (window.isOpen())
        {
            sf::Event event;
            while (window.pollEvent(event))
            {
                if (event.type == sf::Event::Closed)
                    window.close();
                else if (event.type == sf::Event::KeyPressed)
                {
                    auto keyAndSound = notes.find(event.key.code);
                    if (keyAndSound != notes.end())
                    {
                        keyAndSound->second.setVolume(100);
                        // std::cout << "about to play, buffer sample count: " << keyAndSound->second.getBuffer()->getSampleCount() << std::endl;
                        keyAndSound->second.play();
                    }
                }
                else if (event.type == sf::Event::KeyReleased)
                {
                    auto keyAndSound = notes.find(event.key.code);
                    if (keyAndSound != notes.end())
                    {
                        // anything below 49.5 will fade and then stop
                        keyAndSound->second.setVolume(49);
                        // std::cout << "sound addr: " << &(keyAndSound->second) << std::endl;
                    }
                }
            }

            // volume envelopes
            for (auto& keyAndSound : notes)
            {
                if (keyAndSound.second.getStatus() == sf::SoundSource::Playing)
                {
                    // std::cout << "sound addr adj volume: " << &(keyAndSound.second) << std::endl;
                    double vol = keyAndSound.second.getVolume();
                    if (vol > 50)
                    {  // articulation
                        // std::cout << "vol == " << vol << std::endl;
                        keyAndSound.second.setVolume(vol - 0.03125);  // 1/32  timed with fps
                    }
                    else if (vol < 49.5 && vol > 0.5)
                    {  // fade out
                        keyAndSound.second.setVolume(vol - 0.25);  // 1/4
                    }
                    else if (vol <= 0.5)
                    {
                        keyAndSound.second.stop();
                    }
                }
            }

            window.clear();
            window.display();  // just to control fps for fade timing
        }

    }

    /** h is half steps away from A 440
        transpose will also be added */
    double getFreq(const double h)
    {
        return 440.0 * pow(2, (transpose + h) / 12.0);
    }

    /** sin wave
        amplitude 4096 / max signed 16 bit int
        1 wavelength at the given frequency (to be repeated)

        frequencies are ceilinged to a fraction of the sample rate
        A 440.00 -> 44100/100 = 441
        C 261.63 -> 44100/168 = 262.5 */
    void makeNote(const double& freq, const sf::Keyboard::Key& key)
    {
        constexpr double max_wavelengths = 24.0;

        size_t closestCross_i = 0;
        sf::Int16 closestCross = 32767;  // max int16

        std::vector<sf::Int16> samples;
        for (size_t x = 0; x < size_t(max_wavelengths * sampleRate / freq); ++x)
        {
            samples.push_back(int(4096 * sin(2.0 * pi * freq * x / sampleRate)));

            // compare to closest cross
            if ((x > 0) && (samples[samples.size() - 2] < 0) && (samples[samples.size() - 1] >= 0))
            {
                // std::cout << "cross with freq " << freq << std::endl;
                // std::cout << "closest " << closestCross << std::endl;
                // std::cout << "values " << samples[samples.size() - 2] << " " << samples[samples.size() - 1] << std::endl;
                if (abs(samples[samples.size() - 2]) < abs(closestCross))
                {
                    closestCross_i = samples.size() - 2;
                    closestCross = samples[samples.size() - 2];
                }
                if (abs(samples[samples.size() - 1]) < closestCross)
                {
                    closestCross_i = samples.size() - 1;
                    closestCross = samples[samples.size() - 1];
                }
            }
        }

        size_t bufferIndex = notes.size();

        /* debugging
        if (key == sf::Keyboard::Z)
        {
            std::cout << "bufferIndex: " << bufferIndex << std::endl;
            std::cout << "sample 0: " << samples[0] << std::endl;
            std::cout << "sample 1: " << samples[1] << std::endl;
            std::cout << "sample 2: " << samples[2] << std::endl;
        }*/
        
        noteBuffers[bufferIndex].loadFromSamples(&(samples[0]),
                                                 closestCross_i,
                                                 1,
                                                 sampleRate);
        // std::cout << "took " << closestCross_i << " out of " << samples.size() << std::endl;
        // std::cout << "value " << closestCross << std::endl;
        // std::cout << "wavelengths taken: " << closestCross_i * max_wavelengths / samples.size() << std::endl;
        sf::Sound* thisNote = &(notes[key]);
        thisNote->setBuffer(noteBuffers[bufferIndex]);
        thisNote->setLoop(true);
        /* thisNote.play();
        if (key == sf::Keyboard::Z)
        {
            std::cout << "buffer address from array right after setting: " << &(noteBuffers[0]) << std::endl;
            std::cout << "buffer address from note right after setting: " << thisNote->getBuffer() << std::endl;
        }
        // sf::sleep(sf::milliseconds(2000));*/
    }

    void makeNotes()
    {
        // adding any notes to this needs to change the size of the buffers array

        makeNote(getFreq(-9), sf::Keyboard::Z);  // middle C
        makeNote(getFreq(-8), sf::Keyboard::S);
        makeNote(getFreq(-7), sf::Keyboard::X);
        makeNote(getFreq(-6), sf::Keyboard::D);
        makeNote(getFreq(-5), sf::Keyboard::C);
        makeNote(getFreq(-4), sf::Keyboard::V);
        makeNote(getFreq(-3), sf::Keyboard::G);
        makeNote(getFreq(-2), sf::Keyboard::B);
        makeNote(getFreq(-1), sf::Keyboard::H);
        makeNote(getFreq(0), sf::Keyboard::N);  // A 440
        makeNote(getFreq(1), sf::Keyboard::J);
        makeNote(getFreq(2), sf::Keyboard::M);

        // duplicate a few notes between the 2 levels
        makeNote(getFreq(3), sf::Keyboard::Comma);
        makeNote(getFreq(4), sf::Keyboard::L);
        makeNote(getFreq(5), sf::Keyboard::Period);
        makeNote(getFreq(6), sf::Keyboard::SemiColon);
        makeNote(getFreq(7), sf::Keyboard::Slash);

        makeNote(getFreq(3), sf::Keyboard::Q);
        makeNote(getFreq(4), sf::Keyboard::Num2);
        makeNote(getFreq(5), sf::Keyboard::W);
        makeNote(getFreq(6), sf::Keyboard::Num3);
        makeNote(getFreq(7), sf::Keyboard::E);
        makeNote(getFreq(8), sf::Keyboard::R);
        makeNote(getFreq(9), sf::Keyboard::Num5);
        makeNote(getFreq(10), sf::Keyboard::T);
        makeNote(getFreq(11), sf::Keyboard::Num6);
        makeNote(getFreq(12), sf::Keyboard::Y);
        makeNote(getFreq(13), sf::Keyboard::Num7);
        makeNote(getFreq(14), sf::Keyboard::U);

        makeNote(getFreq(15), sf::Keyboard::I);
        makeNote(getFreq(16), sf::Keyboard::Num9);
        makeNote(getFreq(17), sf::Keyboard::O);
        makeNote(getFreq(18), sf::Keyboard::Num0);
        makeNote(getFreq(19), sf::Keyboard::P);
        makeNote(getFreq(20), sf::Keyboard::LBracket);
        makeNote(getFreq(21), sf::Keyboard::Equal);
        makeNote(getFreq(22), sf::Keyboard::RBracket);
        makeNote(getFreq(23), sf::Keyboard::BackSpace);
        // keyboard layouts differ in what key is beside right bracket
        makeNote(getFreq(24), sf::Keyboard::BackSlash);
        makeNote(getFreq(24), sf::Keyboard::Return);
    }
};

int main()
{
    Piano p;

    p.run();

    return 0;
}
