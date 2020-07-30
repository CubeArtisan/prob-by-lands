////////////////////////////////////////////////////////////////////////////////
//
//  SYCL Sample Code
//
//  Author: Codeplay, April 2019
//
////////////////////////////////////////////////////////////////////////////////

/*!
If you encounter an application cannot start because SYCL.dll or SYCL_d.dll is
missing error, then you may have to restart your computer in order to update the
environment variable COMPUTECPPROOT.
*/

#include <CL/sycl.hpp>

#include <PRNG/TinyMT.hpp>

#include <iomanip>
#include <iostream>
#include <random>
#include <vector>

using namespace cl::sycl;

struct Requirement {
    int numberOfCards;
    int mNumberOfLands;
    int mCmc;
    std::array<int, 2> colorRequirements;
    std::array<int, 3> landCounts;
    std::random_device::result_type seed;
};

struct Deck {

    Deck(int numberOfLands, int numberOfCards, std::array<int, 3> numberOfDesiredLands, std::random_device::result_type seed)
        : _numberOfLands(numberOfLands), _numberOfCards(numberOfCards), _numberOfDesiredLands(numberOfDesiredLands),
          _generator{ seed }
    { }

    int drawCard() {
        std::uniform_int_distribution<int> distribution(1, _numberOfCards);
        const int cardIndex = distribution(_generator);
        int desiredLandCutoff = 0;
        _numberOfCards--;
        for (int i=0; i < _numberOfDesiredLands.size(); i++) {
            desiredLandCutoff += _numberOfDesiredLands[i];
            if (cardIndex <= desiredLandCutoff) {
                _numberOfDesiredLands[i]--;
                _numberOfLands--;
                return i;
            }
        }
        if (cardIndex <= _numberOfLands) {
            _numberOfLands--;
            return _numberOfDesiredLands.size();
        } else {
            return -1;
        }
    }

private:
    int _numberOfLands;
    std::array<int, 3> _numberOfDesiredLands;
    int _numberOfCards;
    prng::tinymt_32 _generator;
};

double processRequirement(Requirement requirement) {
    prng::tinymt_32 generator { requirement.seed };
    int ITERATIONS = 1'000'000;
    int CARDS_IN_HAND = 7;
    double countOK = 0;
    double countConditional = 0;
    for (int i=0; i < ITERATIONS; i++) {
        bool mulligan = true;
        int startingHandSize = CARDS_IN_HAND + 1;
        int landsInHand = 0;
        std::array<int, 4> inHand{0, 0, 0, 0};
        Deck deck(requirement.mNumberOfLands, requirement.numberOfCards, requirement.landCounts, generator());
        while (mulligan && startingHandSize > 2) {
            deck = Deck(requirement.mNumberOfLands, requirement.numberOfCards, requirement.landCounts, generator());
            landsInHand = 0;
            inHand = std::array<int, 4>{0, 0, 0, 0};
            for (int j = 1; j <= CARDS_IN_HAND; j++) {
                int cardType = deck.drawCard();
                if (cardType >= 0) {
                    landsInHand++;
                    inHand[cardType]++;
                }
            }
            mulligan = landsInHand < 2 || landsInHand > 5;
            startingHandSize--;
        }
        if (startingHandSize < 7) {
            landsInHand = std::min(landsInHand, startingHandSize);
            inHand[2] = std::min(landsInHand, inHand[3]);
            int remaining = landsInHand - inHand[3];
            if (inHand[0] + inHand[1] > remaining) {
                int total = requirement.colorRequirements[0] + requirement.colorRequirements[1];
                inHand[0] = std::min(inHand[0], remaining * requirement.colorRequirements[0] / total);
                inHand[1] = std::min(inHand[1], remaining * requirement.colorRequirements[1] / total);
            }
        }

        for (int turn = 2; turn <= requirement.mCmc; turn++) {
            int cardType = deck.drawCard();
            if (cardType >= 0) {
                landsInHand++;
                inHand[cardType]++;
            }
        }
        if (landsInHand >= requirement.mCmc) {
            countConditional += 1;
            int landsForA = std::min(requirement.colorRequirements[0], inHand[0]);
            int landsForB = std::min(requirement.colorRequirements[1], inHand[1]);
            if (inHand[2] >= requirement.colorRequirements[0] + requirement.colorRequirements[1] - landsForA - landsForB) {
                countOK += 1;
            }
        }
    }
    return countOK / countConditional;
}

constexpr int NUMBER_OF_LANDS = 17;
constexpr int NUMBER_OF_CARDS = 40;

class process_requirement;

int main() {
    std::default_random_engine generator;
    std::vector<Requirement> requirements;
    requirements.reserve(55216);
    for (int cmc=1; cmc <= 8; cmc++) {
        for (int i = 1; i <= std::min(cmc, 6); i++) {
            for (int desiredLands = i; desiredLands <= NUMBER_OF_LANDS; desiredLands++) {
                requirements.push_back(Requirement{NUMBER_OF_CARDS, NUMBER_OF_LANDS, cmc, { i, 0 }, { desiredLands, 0, 0 }});
            }
            for (int j=1;  j <= std::min(cmc - i, std::min(3, i)); j++) {
                for (int numberLandA = 0; numberLandA <= NUMBER_OF_LANDS; numberLandA++) {
                    for (int numberLandB = 0; numberLandB <= NUMBER_OF_LANDS - numberLandA; numberLandB++) {
                        for (int numberLandAB = 0; numberLandAB <= NUMBER_OF_LANDS - numberLandA - numberLandB; numberLandAB++) {
                            requirements.push_back({ NUMBER_OF_CARDS, NUMBER_OF_LANDS, cmc, { i, j }, { numberLandA, numberLandB, numberLandAB }, generator() }); 
                        }
                    }
                }
            }
        }
    }
    std::cout << requirements.size() << std::endl;
    std::vector<double> output(requirements.size());

    /* Wrap all SYCL structures and function calls with a try-catch block to catch
     * SYCL exceptions. */
    try {
        const auto dev_type = cl::sycl::info::device_type::gpu;
        std::cout << "Getting platforms" << std::endl;
        // Platform selection
        auto plats = cl::sycl::platform::get_platforms();
        std::cout << "Retrieved plats" << std::endl;
        std::cout << "Empty: " << plats.empty() << std::endl;
        if (plats.empty()) throw std::runtime_error{ "No OpenCL platform found." };

        std::cout << "Found platforms:" << std::endl;
        for (const auto plat : plats) std::cout << "\t" << plat.get_info<cl::sycl::info::platform::vendor>() << std::endl;

        auto plat = plats.at(1);

        std::cout << "\n" << "Selected platform: " << plat.get_info<cl::sycl::info::platform::vendor>() << std::endl;

        std::cout << "Getting devices" << std::endl;
        // Device selection
        auto devs = plat.get_devices();
        std::cout << "Empty: " << devs.empty() << std::endl;
        if (devs.empty()) throw std::runtime_error{ "No OpenCL device of specified type found on selected platform." };

        auto dev = devs.front();

        std::cout << "Selected device: " << dev.get_info<cl::sycl::info::device::name>() << "\n" << std::endl;

        // Context, queue, buffer creation
        auto async_error_handler = [](cl::sycl::exception_list errors) { for (auto error : errors) throw error; };

        cl::sycl::context ctx{ dev, async_error_handler };

        cl::sycl::queue myQueue{ dev };

        std::cout << "Created queue" << std::endl;

        /* Create a scope to control data synchronisation of buffer objects. */
        {
            /* Create 1 dimensionsal buffers for the input and output data of size
             * 1024. */
            buffer<Requirement, 1> inputBuffer(requirements.data(), range<1>(requirements.size()));
            buffer<double, 1> outputBuffer(output.data(), range<1>(requirements.size()));

            /* Submit a command_group to execute from the queue. */
            myQueue.submit([&](handler &cgh) {

                /* Create accessors for accessing the input and output data within the
                 * kernel. */
                auto inputPtr = inputBuffer.get_access<access::mode::read>(cgh);
                auto outputPtr = outputBuffer.get_access<access::mode::write>(cgh);
                std::cout << "Created pointers" << std::endl;
                cgh.parallel_for<class process_requirement>(range<1>(requirements.size()),
                    [=](cl::sycl::id<1> wiID) {
                        outputPtr[wiID] = processRequirement(inputPtr[wiID]);
                    }
                );
            });
        }

    } catch (exception e) {

      /* In the case of an exception being throw, print theerror message and
       * return 1. */
      std::cout << e.what();
      return 1;
    }
    std::cout.precision(12);
    /* Sum up all the values in the output array. */
    int sum = 0;
    for (int i = 0; i < requirements.size(); i++) {
        const Requirement& requirement = requirements[i];
        std::cout  << requirement.numberOfCards << "," << requirement.mNumberOfLands << "," << requirement.mCmc << ","
                   << requirement.colorRequirements[0] << "," << requirement.colorRequirements[1] << ","
                   << requirement.landCounts[0] << "," << requirement.landCounts[1] << "," << requirement.landCounts[2]
                   << "," << output[i] << "\n";
    }

    return 0;
}