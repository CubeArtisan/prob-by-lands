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

/* #include <CL/sycl.hpp> */

#include <fstream>
#include <iomanip>
#include <iostream>
#include <iterator>
#include <optional>
#include <random>
#include <vector>

#include <PRNG/TinyMT.hpp>
#include <blockingconcurrentqueue.h>

/* using namespace cl::sycl; */

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
        const int cardIndex = std::uniform_int_distribution<int>(1,_numberOfCards)(_generator);
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

/* class GPUDeviceSelector : public cl::sycl::device_selector { */
/* public: */
/*     int operator()(const cl::sycl::device &Device) const override { */
/*         using namespace cl::sycl::info; */

/*         const std::string DeviceName = Device.get_info<cl::sycl::info::device::name>(); */
/*         const std::string DeviceVendor = Device.get_info<cl::sycl::info::device::vendor>(); */
/*         std::cout << "Found device with name " << DeviceName << " and vendor " << DeviceVendor << std::endl; */

/*         return Device.is_gpu(); */
/*     } */
/* }; */
constexpr size_t ITERATIONS = 650'000;
constexpr int CARDS_IN_HAND = 7;
double processRequirement(Requirement requirement) {
    prng::tinymt_32 generator { requirement.seed };
    double countOK = 0;
    double countCMC = 0;
    while (countCMC < ITERATIONS) {
        bool mulligan = true;
        int startingHandSize = CARDS_IN_HAND + 1;
        int landsInHand = 0;
        std::array<int, 4> inHand{0, 0, 0, 0};
        Deck deck(requirement.mNumberOfLands, requirement.numberOfCards, requirement.landCounts, static_cast<unsigned int>(generator()));
        while (mulligan && startingHandSize > 3) {
            deck = Deck(requirement.mNumberOfLands, requirement.numberOfCards, requirement.landCounts, static_cast<unsigned int>(generator()));
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
            inHand[2] = std::min(landsInHand, inHand[2]);
            int remaining = landsInHand - inHand[2];
            if (inHand[0] + inHand[1] > remaining) {
                int total = std::max(requirement.colorRequirements[0] + requirement.colorRequirements[1], 1);
                inHand[0] = std::min(inHand[0], remaining * requirement.colorRequirements[0] / total);
                inHand[1] = std::min(inHand[1], remaining * requirement.colorRequirements[1] / total);
            }
        }

        for (int turn = 0; turn < requirement.mCmc; turn++) {
            int cardType = deck.drawCard();
            if (cardType >= 0) {
                landsInHand++;
                inHand[cardType]++;
            }
        }
        if (landsInHand >= requirement.mCmc) {
            countCMC += 1;
            int landsForA = std::max(0, requirement.colorRequirements[0] - inHand[0]);
            int landsForB = std::max(0, requirement.colorRequirements[1] - inHand[1]);
            if (inHand[2] >=  landsForA + landsForB) {
                countOK += 1;
            }
        }
    }
    return countOK / ITERATIONS;
}

constexpr size_t NUM_CMC = 9;
constexpr size_t NUM_REQUIRED_A = 7;
constexpr size_t NUM_REQUIRED_B = 4;
constexpr size_t NUMBER_OF_LANDS = 17;
constexpr int NUMBER_OF_CARDS = 40;

class process_requirement;

std::array<float, NUM_CMC * NUM_REQUIRED_A * NUM_REQUIRED_B * (NUMBER_OF_LANDS + 1) * (NUMBER_OF_LANDS + 1) * (NUMBER_OF_LANDS + 1)> probTable{-1};
int main() {
    std::default_random_engine generator;
    std::vector<Requirement> requirements;
    for (int cmc=0; cmc < NUM_CMC; cmc++) {
        for (int i = 0; i < NUM_REQUIRED_A; i++) {
            for (int j=0;  j < NUM_REQUIRED_B; j++) {
                for (int numberLandA = 0; numberLandA <= NUMBER_OF_LANDS; numberLandA++) {
                    for (int numberLandB = 0; numberLandB <= NUMBER_OF_LANDS; numberLandB++) {
                        for (int numberLandAB = 0; numberLandAB <= NUMBER_OF_LANDS; numberLandAB++) {
                            requirements.push_back({ NUMBER_OF_CARDS, NUMBER_OF_LANDS, cmc, { i, j }, { numberLandA, numberLandB, numberLandAB }, static_cast<unsigned int>(generator()) }); 
                        }
                    }
                }
            }
        }
    }
    /* Wrap all SYCL structures and function calls with a try-catch block to catch
     * SYCL exceptions. */
    // try {
        /* const auto dev_type = cl::sycl::info::device_type::gpu; */
        /* std::cout << "Getting platforms" << std::endl; */
        // Platform selection
//        auto plats = cl::sycl::platform::get_platforms();
//        std::cout << "Retrieved plats" << std::endl;
//        std::cout << "Empty: " << plats.empty() << std::endl;
//        // if (plats.empty()) throw std::runtime_error{ "No OpenCL platform found." };
//
//        std::cout << "Found platforms:" << std::endl;
//        for (const auto plat : plats) std::cout << "\t" << plat.get_info<cl::sycl::info::platform::vendor>() << std::endl;
////
//        auto plat = plats.at(0);
////
////        std::cout << "\n" << "Selected platform: " << plat.get_info<cl::sycl::info::platform::vendor>() << std::endl;
//
//        std::cout << "Getting devices" << std::endl;
//        // Device selection
//        auto devs = plat.get_devices();
//        std::cout << "Empty: " << devs.empty() << std::endl;
//        // if (devs.empty()) throw std::runtime_error{ "No OpenCL device of specified type found on selected platform." };
//
//        auto dev = devs.front();

//        std::cout << "Selected device: " << dev.get_info<cl::sycl::info::device::name>() << "\n" << std::endl;

        // Context, queue, buffer creation
        // auto async_error_handler = [](cl::sycl::exception_list errors) { for (auto error : errors) throw error; };

        /* cl::sycl::queue myQueue{ GPUDeviceSelector() }; */

        /* std::cout << "Created queue" << std::endl; */

        /* /1* Create a scope to control data synchronisation of buffer objects. *1/ */
        /* { */
        /*     /1* Create 1 dimensionsal buffers for the input and output data of size */
        /*      * 1024. *1/ */
        /*     buffer<Requirement, 1> inputBuffer(requirements.data(), range<1>(requirements.size())); */
        /*     buffer<double, 1> outputBuffer(output.data(), range<1>(requirements.size())); */

        /*     /1* Submit a command_group to execute from the queue. *1/ */
        /*     myQueue.submit([&](handler &cgh) { */

        /*         /1* Create accessors for accessing the input and output data within the */
        /*          * kernel. *1/ */
        /*         auto inputPtr = inputBuffer.get_access<access::mode::read>(cgh); */
        /*         auto outputPtr = outputBuffer.get_access<access::mode::write>(cgh); */
        /*         std::cout << "Created pointers" << std::endl; */
        /*         cgh.parallel_for<class process_requirement>(range<1>(requirements.size()), */
        /*             [=](cl::sycl::id<1> wiID) { */
        /*                 outputPtr[wiID] = processRequirement(inputPtr[wiID]); */
        /*             } */
        /*         ); */
        /*     }); */
        /* } */

    moodycamel::BlockingConcurrentQueue<std::optional<std::size_t>> task_queue;
    std::vector<double> output(requirements.size());
    const std::size_t one_percent = requirements.size() / 100;

    auto processResults = [&]() {
        std::optional<std::size_t> task;
        while(true) {
            task_queue.wait_dequeue(task);
            if (!task) return;
            const std::size_t index = *task;
            if (index % one_percent == 0) {
                std::cout << "Finished " << index / one_percent << " percent.\n";
            }
            output[index] = processRequirement(requirements[index]);
        }
    };

    std::vector<std::optional<std::size_t>> tasks;
    tasks.reserve(requirements.size() + 32);
    for (size_t i=0; i < requirements.size(); i++) tasks.emplace_back(i);
    std::vector<std::thread> workers;
    workers.reserve(32);
    for (size_t i=0; i < 32; i++) {
        workers.emplace_back(processResults);
        tasks.push_back(std::nullopt);
    }
    task_queue.enqueue_bulk(std::make_move_iterator(tasks.begin()), tasks.size());

    for (auto& worker : workers) worker.join();
    workers.clear();


//    } catch (exception e) {
//
//      /* In the case of an exception being throw, print theerror message and
//       * return 1. */
//      std::cout << e.what();
//      return 1;
//    }
    std::cout.precision(12);

    /* Sum up all the values in the output array. */
    for (size_t i = 0; i < requirements.size(); i++) {
        probTable[
            requirements[i].landCounts[0]
              + (NUMBER_OF_LANDS + 1) * (requirements[i].landCounts[1]
                + (NUMBER_OF_LANDS + 1) * (requirements[i].landCounts[2]
                  + (NUMBER_OF_LANDS + 1) * (requirements[i].colorRequirements[1]
                    + NUM_REQUIRED_B * (requirements[i].colorRequirements[0]
                      + NUM_REQUIRED_A * requirements[i].mCmc))))] = static_cast<float>(output[i]);
    }
    std::ofstream probTableFile("probTable.bin", std::ios::binary);
    probTableFile.write(reinterpret_cast<char*>(probTable.data()), sizeof(probTable));
    std::ofstream probTableCsv("probTable.csv");
    for (std::size_t i = 0; i < requirements.size(); i++) {
        const Requirement& requirement = requirements[i];
        probTableCsv << requirement.numberOfCards << "," << requirement.mNumberOfLands << "," << requirement.mCmc << ","
                     << requirement.colorRequirements[0] << "," << requirement.colorRequirements[1] << ","
                     << requirement.landCounts[0] << "," << requirement.landCounts[1] << "," << requirement.landCounts[2]
                     << "," << output[i] << "\n";
    }

    return 0;
}
