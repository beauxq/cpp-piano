#include <SFML/Graphics.hpp>
#include <SFML/Audio.hpp>

#include <cmath>
#include <unordered_map>
#include <vector>
#include <list>
#include <algorithm>  // min

// #include <iostream>  // debugging

constexpr double pi = 3.14159265358979323846;

typedef int Step;

struct Piano
{
    const int sampleRate = 44100;
    Step transpose;
    const bool stretchedTuning;

    // using linked list (rather than vector) to make sure they don't move
    std::list<sf::SoundBuffer> noteBuffers;

    /** value is number of half steps from A 440, not transposed */
    static const std::unordered_map<sf::Keyboard::Key, Step> keysToSteps;
    /** transposed step mapped to sound */
    std::unordered_map<Step, sf::Sound> stepsToSounds;

    /** transposed step being held down */
    std::unordered_map<sf::Keyboard::Key, Step> keysToStepsHeld;

    sf::RenderWindow window;

    Piano(const Step transpose=0, const bool stretchedTuning=true) : transpose(transpose),
                                                                     stretchedTuning(stretchedTuning)
    {
        // std::cout << "in ctor, before makeNotes" << std::endl;
        makeNotes();
        /* debugging
        std::cout << "made notes, size: " << stepsToSounds.size() << std::endl;
        std::cout << "buffers size: " << noteBuffers.size() << std::endl;
        std::cout << "buffer sample count: " << noteBuffers.front().getSampleCount() << std::endl;
        std::cout << "buffer contents: " << noteBuffers.front().getSamples()[1] << std::endl;
        // noteBuffers made from unordered collection so anything could be in front
        std::cout << "buffer address from note: " << stepsToSounds[-9].getBuffer() << std::endl;
        std::cout << "buffer sample count from note: " << stepsToSounds[-9].getBuffer()->getSampleCount() << std::endl;
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
                    auto keyAndStep = keysToSteps.find(event.key.code);
                    if (keyAndStep != keysToSteps.end())
                    {
                        Step transposedStep = keyAndStep->second + transpose;
                        sf::Sound* sound = &(stepsToSounds[transposedStep]);
                        sound->setVolume(100);
                        // std::cout << "about to play, buffer sample count: " << sound->getBuffer()->getSampleCount() << std::endl;
                        sound->play();

                        keysToStepsHeld[keyAndStep->first] = transposedStep;
                    }
                    else if (event.key.code == sf::Keyboard::Up)
                    {
                        Step newTranspose = transpose + 12;
                        transpose = newTranspose > 36 ? transpose : newTranspose;
                        makeNotes();
                    }
                    else if (event.key.code == sf::Keyboard::Down)
                    {
                        Step newTranspose = transpose - 12;
                        transpose = newTranspose < -48 ? transpose : newTranspose;
                        makeNotes();
                    }
                }
                else if (event.type == sf::Event::KeyReleased)
                {
                    auto keyAndTransposedStep = keysToStepsHeld.find(event.key.code);
                    if (keyAndTransposedStep != keysToStepsHeld.end())
                    {
                        sf::Sound* sound = &(stepsToSounds[keyAndTransposedStep->second]);
                        // anything below 49.5 will fade and then stop
                        sound->setVolume(49);
                        // std::cout << "sound addr: " << sound << std::endl;
                    }
                }
            }

            // volume envelopes
            for (auto& stepAndSound : stepsToSounds)
            {
                if (stepAndSound.second.getStatus() == sf::SoundSource::Playing)
                {
                    // std::cout << "sound addr adj volume: " << &(stepAndSound.second) << std::endl;
                    double vol = stepAndSound.second.getVolume();
                    if (vol > 50)
                    {  // articulation
                        // std::cout << "vol == " << vol << std::endl;
                        stepAndSound.second.setVolume(vol - 0.03125);  // 1/32  timed with fps
                    }
                    else if (vol < 49.5 && vol > 0.5)
                    {  // fade out
                        stepAndSound.second.setVolume(vol - 0.25);  // 1/4
                    }
                    else if (vol <= 0.5)
                    {
                        stepAndSound.second.stop();
                    }
                }
            }

            window.clear();
            window.display();  // just to control fps for volume envelopes
        }

    }

    /** h is half steps away from A 440 */
    double getFreq(const double& h)
    {
        // Wikipedia says stretched tuning is 35 cents over half the piano.
        // It says it in a confusing way that I might be misinterpreting,
        // and cites a source that no longer exists.
        // 35 cents over 4 octaves is
        // (2 ** (4835 / 1200)) ** 0.25
        // = 2.0101339843933212 (per octave)
        return 440.0 * pow(stretchedTuning ? 2.0101339843933212 : 2.0, h / 12.0);
    }

    /** soft, rounded clip on amplitude
     *  sin wave won't work at low frequencies, but simple clipping is too drastic
    */
    static sf::Int16 amplitudeCap(const double& amp)
    {
        double abs_amp = abs(amp);
        // sigmoid
        double t = 1.0 / (1.0 + exp(-((abs_amp - 28000.0) / 5000.0)));
        // inverted sigmoid
        double s = 1.0 - t;
        // lower bound for large amp
        double g = 28000.0 / abs_amp;
        // space between 1 and lower bound
        double h = 1 - g;
        // inverted sigmoid squished in h
        double o = h * s + g;
        // gradual shift from unbounded inverted sigmoid to squished inverted sigmoid
        double p = t * o + pow(s, 1.5);
        return amp * std::min(1.0, p);
    }

    /** sin wave at the given frequency
        amplitude cap out of max signed 16 bit int
        find number of wavelengths to make a smooth loop

        if max_wavelengths is 1,
        frequencies are ceilinged to a fraction of the sample rate
        A 440.00 -> 44100/100 = 441
        C 261.63 -> 44100/168 = 262.5 */
    void makeNote(const Step& step)
    {
        constexpr double max_wavelengths = 24.0;
        const double freq = getFreq(step);
        const double max_amplitude = 5e9 / pow(freq + 45.0, 2.5) + 3500.0;
        // TODO: not happy with this ^ g2 to d3 is a little low

        size_t closestCross_i = size_t(sampleRate / freq);
        sf::Int16 closestCross = 32767;  // max int16

        std::vector<sf::Int16> samples;
        for (size_t x = 0; x < size_t(max_wavelengths * sampleRate / freq); ++x)
        {
            double value = max_amplitude * sin(2.0 * pi * freq * x / sampleRate);
            samples.push_back(amplitudeCap(value));

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

        // TODO: try catch bad_alloc
        noteBuffers.emplace_back();

        /* debugging
        if (step == -22 || step == -23)
        {
            std::cout << "max amp: " << max_amplitude << std::endl;
            std::cout << "buffer count: " << noteBuffers.size() << std::endl;
            std::cout << "sample 0: " << samples[0] << std::endl;
            std::cout << "sample 1: " << samples[1] << std::endl;
            std::cout << "sample 2: " << samples[2] << std::endl;
        }*/
        
        noteBuffers.back().loadFromSamples(&(samples[0]),
                                           closestCross_i,
                                           1,
                                           sampleRate);
        // std::cout << "took " << closestCross_i << " out of " << samples.size() << std::endl;
        // std::cout << "value " << closestCross << std::endl;
        // std::cout << "wavelengths taken: " << closestCross_i * max_wavelengths / samples.size() << std::endl;
        sf::Sound* thisSound = &(stepsToSounds[step]);
        thisSound->setBuffer(noteBuffers.back());
        thisSound->setLoop(true);
        /* thisSound.play();
        if (step == -9)  // middle C
        {
            std::cout << "buffer address from array right after setting: " << &(noteBuffers.back()) << std::endl;
            std::cout << "buffer address from note right after setting: " << thisSound->getBuffer() << std::endl;
        }
        // sf::sleep(sf::milliseconds(2000));*/
    }

    void makeNotes()
    {
        for (auto& keyAndStep : keysToSteps)
        {
            Step transposed = keyAndStep.second + transpose;
            sf::Sound* sound = &(stepsToSounds[transposed]);
            if (sound->getBuffer() == nullptr)
            {
                makeNote(transposed);
            }
        }
    }
};

const std::unordered_map<sf::Keyboard::Key, Step> Piano::keysToSteps = {
    {sf::Keyboard::Z, -9},  // middle C
    {sf::Keyboard::S, -8},
    {sf::Keyboard::X, -7},
    {sf::Keyboard::D, -6},
    {sf::Keyboard::C, -5},
    {sf::Keyboard::V, -4},
    {sf::Keyboard::G, -3},
    {sf::Keyboard::B, -2},
    {sf::Keyboard::H, -1},
    {sf::Keyboard::N, 0},  // A 440
    {sf::Keyboard::J, 1},
    {sf::Keyboard::M, 2},

    // duplicate a few notes between the 2 levels
    {sf::Keyboard::Comma, 3},
    {sf::Keyboard::L, 4},
    {sf::Keyboard::Period, 5},
    {sf::Keyboard::SemiColon, 6},
    {sf::Keyboard::Slash, 7},

    {sf::Keyboard::Q, 3},
    {sf::Keyboard::Num2, 4},
    {sf::Keyboard::W, 5},
    {sf::Keyboard::Num3, 6},
    {sf::Keyboard::E, 7},
    {sf::Keyboard::R, 8},
    {sf::Keyboard::Num5, 9},
    {sf::Keyboard::T, 10},
    {sf::Keyboard::Num6, 11},
    {sf::Keyboard::Y, 12},
    {sf::Keyboard::Num7, 13},
    {sf::Keyboard::U, 14},

    {sf::Keyboard::I, 15},
    {sf::Keyboard::Num9, 16},
    {sf::Keyboard::O, 17},
    {sf::Keyboard::Num0, 18},
    {sf::Keyboard::P, 19},
    {sf::Keyboard::LBracket, 20},
    {sf::Keyboard::Equal, 21},
    {sf::Keyboard::RBracket, 22},
    {sf::Keyboard::BackSpace, 23},
    // keyboard layouts differ in what key is beside right bracket
    {sf::Keyboard::BackSlash, 24},
    {sf::Keyboard::Return, 24}
};

int main()
{
    Piano p;

    p.run();

    return 0;
}
