#include <array>
#include <iostream>
#include <sstream>
#include <mutex>
#include <random>
#include <thread>
#include <vector>

/** Thread safe cout class
  * Exemple of use:
  *    PrintThread{} << "Hello world!" << std::endl;
  */
class PrintThread: public std::ostringstream
{
public:
    PrintThread() = default;

    ~PrintThread()
    {
        std::lock_guard<std::mutex> guard(_mutexPrint);
        std::cout << this->str() << std::endl;
    }

private:
    static std::mutex _mutexPrint;
};

std::mutex PrintThread::_mutexPrint{};
struct Requirement {
    int numberOfCards;
    int mNumberOfLands;
    int mCmc;
    std::array<int, 2> colorRequirements;
    std::array<int, 3> landCounts;
};

std::default_random_engine generator;

struct Deck {

    Deck(int numberOfLands, int numberOfCards, std::array<int, 3> numberOfDesiredLands)
        : _numberOfLands(numberOfLands), _numberOfCards(numberOfCards), _numberOfDesiredLands(numberOfDesiredLands)
    { }

    int drawCard() {
        std::uniform_int_distribution<int> distribution(1, _numberOfCards);
        const int cardIndex = distribution(generator);
        int desiredLandCutoff = 0;
        _numberOfCards--;
        for (int i=0; i < _numberOfDesiredLands.size(); i++) {
            desiredLandCutoff += _numberOfDesiredLands[i];
            if (cardIndex <= desiredLandCutoff) {
                _numberOfDesiredLands[i]--;
                _numberOfLands--;
                return i + 1;
            }
        }
        if (cardIndex <= _numberOfLands) {
            _numberOfLands--;
            return _numberOfDesiredLands.size() + 1;
        } else {
            return -1;
        }
    }

private:
    int _numberOfLands;
    std::array<int, 3> _numberOfDesiredLands;
    int _numberOfCards;
};

constexpr int ITERATIONS = 1000000;
constexpr int CARDS_IN_HAND = 7;

void processRequirement(const std::vector<Requirement>& requirements) {
    for(const Requirement& requirement : requirements) {
        double countOK = 0;
        double countConditional = 0;
        for (int i=0; i < ITERATIONS; i++) {
            bool mulligan = true;
            int startingHandSize = CARDS_IN_HAND + 1;
            int landsInHand = 0;
            std::array<int, 4> inHand{0, 0, 0, 0};
            Deck deck(requirement.mNumberOfLands, requirement.numberOfCards, requirement.landCounts);
            while (mulligan && startingHandSize > 2) {
                deck = Deck(requirement.mNumberOfLands, requirement.numberOfCards, requirement.landCounts);
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
                inHand[3] = std::min(landsInHand, inHand[3]);
                int remaining = landsInHand - inHand[3];
                if (inHand[1] + inHand[2] > remaining) {
                    int total = requirement.colorRequirements[0] + requirement.colorRequirements[1];
                    inHand[1] = std::min(inHand[1], remaining * requirement.colorRequirements[0] / total);
                    inHand[2] = std::min(inHand[2], remaining * requirement.colorRequirements[1] / total);
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
                int landsForA = std::min(requirement.colorRequirements[0], inHand[1]);
                int landsForB = std::min(requirement.colorRequirements[1], inHand[2]);
                if (inHand[3] >= requirement.colorRequirements[0] + requirement.colorRequirements[1] - landsForA - landsForB) {
                    countOK += 1;
                }
            }
        }
        PrintThread syncCout;
        syncCout  << requirement.numberOfCards << "," << requirement.mNumberOfLands << "," << requirement.mCmc << ","
                  << requirement.colorRequirements[0] << "," << requirement.colorRequirements[1] << ","
                  << requirement.landCounts[0] << "," << requirement.landCounts[1] << "," << requirement.landCounts[2]
                  << "," << countOK / countConditional;
    }
}

constexpr int NUMBER_OF_LANDS = 17;
constexpr int NUMBER_OF_CARDS = 40;

int main() {
    std::vector<Requirement> requirements;
    for (int cmc=1; cmc <= 8; cmc++) {
        for (int i = 1; i <= std::min(cmc, 6); i++) {
            for (int desiredLands = i; desiredLands <= NUMBER_OF_LANDS; desiredLands++) {
                requirements.push_back(Requirement{NUMBER_OF_CARDS, NUMBER_OF_LANDS, cmc, { i, 0 }, { desiredLands, 0, 0 }});
            }
            for (int j=1;  j <= std::min(cmc - i, std::min(3, i)); j++) {
                for (int numberLandA = 0; numberLandA <= NUMBER_OF_LANDS; numberLandA++) {
                    for (int numberLandB = 0; numberLandB <= NUMBER_OF_LANDS - numberLandA; numberLandB++) {
                        for (int numberLandAB = 0; numberLandAB <= NUMBER_OF_LANDS - numberLandA - numberLandB; numberLandAB++) {
                            requirements.push_back({ NUMBER_OF_CARDS, NUMBER_OF_LANDS, cmc, { i, j }, { numberLandA, numberLandB, numberLandAB } }); 
                        }
                    }
                }
            }
        }
    }
    std::cerr << requirements.size() << std::endl;
    const auto processor_count = std::thread::hardware_concurrency() == 0 ? 1 : std::thread::hardware_concurrency();
    std::vector<std::thread> threads;
    for (int i = 0; i < processor_count; i++) {
        std::vector<Requirement> threadRequirements(requirements.begin() + (i * requirements.size() / processor_count), requirements.begin() + ((i + 1) * requirements.size() / processor_count));
        threads.push_back(std::thread(processRequirement, threadRequirements));
    }

    for (std::thread& thread : threads) {
        thread.join();
    }
}
