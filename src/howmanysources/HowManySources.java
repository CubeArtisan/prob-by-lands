package howmanysources;

import java.util.ArrayList;
import java.util.List;
import java.util.Queue;
import java.util.Random;
import java.util.concurrent.*;

class Requirement {
    int mNumberOfCards;
    int mNumberOfLands;
    int mCmc;
    int[] mColorRequirements;

    Requirement(int numberOfCards, int numberOfLands, int cmc, int[] colorRequirements) {
        mNumberOfCards = numberOfCards;
        mNumberOfLands = numberOfLands;
        mCmc = cmc;
        mColorRequirements = colorRequirements;
    }

    int getNumberOfCards() {
        return mNumberOfCards;
    }

    int getNumberOfLands() {
        return mNumberOfLands;
    }

    int getCmc() {
        return mCmc;
    }

    int[] getColorRequirements() {
        return mColorRequirements;
    }
}

public class HowManySources {
    private static final int[] deckSizes = new int[]{40, 60, 99};

    public static void main(String[] args) throws InterruptedException {
        System.out.println("Number of LandA Needed,Number of LandB Needed,CMC,Number Of LandA in Deck,Number of LandB in Deck,Number of LandAB in Deck,Number of Lands In Deck,Number of Cards in Deck,OnCurveProbability");

        List<Requirement> requirements = new ArrayList<>();
        for (int NrCards : deckSizes) {
            int NrLands = 17 * NrCards / 40;
            for (int TurnAllowed = 1; TurnAllowed <= 15; TurnAllowed++) {
                for (int NrGoodLandsNeeded = 1; NrGoodLandsNeeded <= Math.min(6, TurnAllowed); NrGoodLandsNeeded++) {
                    requirements.add(new Requirement(NrCards, NrLands, TurnAllowed, new int[]{NrGoodLandsNeeded}));
                }
                for (int i = 1; i <= Math.min(3, TurnAllowed); i++) {
                    for (int j = 1; j <= Math.min(3, TurnAllowed - i); j++) {
                        requirements.add(new Requirement(NrCards, NrLands, TurnAllowed, new int[]{i, j}));
                        for (int k = 1; k <= Math.min(3, TurnAllowed - i - k); k++) {
                            requirements.add(new Requirement(NrCards, NrLands, TurnAllowed, new int[]{i, j, k}));
                        }
                    }
                }
            }
        }
        List<Thread> threads = new ArrayList<>();
        int cores = Runtime.getRuntime().availableProcessors() - 1;
        Queue<String> outputQueue = new ConcurrentLinkedQueue<>();
        Thread outputThread = new Thread(() -> {
            while (true) {
                String str = outputQueue.poll();
                if (str == null) continue;
                if (str.equals("exit")) return;
                System.out.println(str);
            }
        });
//        outputThread.start();
        ExecutorService pool = Executors.newFixedThreadPool(cores);
        pool.execute(outputThread);
        for (Requirement requirement : requirements) {
            pool.execute(new RequirementsProcessorThread(outputQueue, new Requirement[] { requirement }));
        }
//        for (int i = 0; i < cores; i++) {
//            int endIndex = (i + 1) * requirements.size() / cores;
//           int startIndex = i * requirements.size() / cores;
//            pool.execute(new RequirementsProcessorThread(outputQueue, requirements.subList(startIndex, endIndex).toArray(new Requirement[endIndex - startIndex])));
//            threads.add(new RequirementsProcessorThread(outputQueue, requirements.subList(i * requirements.size() / cores, (i + 1) * requirements.size() / cores)));
//            threads.get(threads.size() - 1).start();
//        }
//        for (Thread thread : threads) {
//            thread.join();
//        }
        outputQueue.add("exit");
//        outputThread.join();
        pool.shutdown();
    }
}

class RequirementsProcessorThread extends Thread {
    private final int NrIterations = 1000000;
    private final Requirement[] mRequirements;
    private final Queue<String> mOutputQueue;

    public RequirementsProcessorThread(Queue<String> outputQueue, Requirement[] requirements) {
        mRequirements = requirements;
        mOutputQueue = outputQueue;
    }

    @Override
    public void run() {
        Deck deck = new Deck();
        for (Requirement requirement : mRequirements) {
            //Declare other variables
            int CardType; //Variable used to reference the type of card drawn from the deck
            int LandsInHand; //This will describe the total amount of lands in your hand
            int GoodLandsInHand; //This will describe the number of lands that can produce the right color in your hand
            int StartingHandSize;
            boolean Mulligan;

            if (requirement.getColorRequirements().length == 1) {
                for (int NrGoodLands = requirement.getColorRequirements()[0]; NrGoodLands <= 17; NrGoodLands++) {
                    double CountOK = 0.0; //This will be the number of relevant games where you draw enough lands and the right colored sources
                    double CountConditional = 0.0; //This will be the number of relevant games where you draw enough lands
                    for (int i = 1; i <= NrIterations; i++) {
                        //Draw opening Hand
                        Mulligan = true;
                        StartingHandSize = 8;
                        LandsInHand = 0;
                        GoodLandsInHand = 0;
                        while (Mulligan && StartingHandSize > 2) {
                            deck.SetDeck(requirement.getNumberOfLands(), NrGoodLands, requirement.getNumberOfCards());
                            LandsInHand = 0;
                            GoodLandsInHand = 0;

                            for (int j = 1; j <= 7; j++) {
                                CardType = deck.DrawCard();
                                if (CardType < 3) {
                                    LandsInHand++;
                                }
                                if (CardType == 1) {
                                    GoodLandsInHand++;
                                }
                            }
                            Mulligan = LandsInHand < 2 || LandsInHand > 5;
                            StartingHandSize--;
                        }
                        if (StartingHandSize < 7) {
                            LandsInHand = Math.min(LandsInHand, StartingHandSize - 1);
                            GoodLandsInHand = Math.min(GoodLandsInHand, LandsInHand);
                        }
                        //For turns 2 on, draw cards for the number of turns available
                        for (int turn = 2; turn <= requirement.getCmc(); turn++) {
                            CardType = deck.DrawCard();
                            if (CardType < 3) {
                                LandsInHand++;
                            }
                            if (CardType == 1) {
                                GoodLandsInHand++;
                            }
                        }

                        if (GoodLandsInHand >= requirement.getColorRequirements()[0] && LandsInHand >= requirement.getCmc()) {
                            CountOK++;
                        }
                        if (LandsInHand >= requirement.getCmc()) {
                            CountConditional++;
                        }
                    }
                    System.out.println(requirement.getColorRequirements()[0] + ",0," + requirement.getCmc() + "," + NrGoodLands + ",0,0," + requirement.getNumberOfLands() + "," + requirement.getNumberOfCards() + "," + CountOK / CountConditional);
                }
            } else if (requirement.getColorRequirements().length == 2) {
                for (int NrLandA = 0; NrLandA <= requirement.getNumberOfLands(); NrLandA++) {
                    for (int NrLandB = 0; NrLandB <= requirement.getNumberOfLands() - NrLandA; NrLandB++) {
                        for (int NrLandAB = 0; NrLandAB <= requirement.getNumberOfLands() - NrLandA - NrLandB; NrLandAB++) {
                            double CountOK = 0.0; //This will be the number of relevant games where you draw enough lands and the right colored sources
                            double CountConditional = 0.0; //This will be the number of relevant games where you draw enough lands
                            for (int i = 1; i <= NrIterations; i++) {
                                //Draw opening Hand
                                Mulligan = true;
                                StartingHandSize = 8;
                                LandsInHand = 0;
                                int[] inHand = new int[6];
                                while (Mulligan && StartingHandSize > 2) {
                                    deck.SetDeck(requirement.getNumberOfLands(), new int[]{NrLandA, NrLandB, NrLandAB}, requirement.getNumberOfCards());
                                    LandsInHand = 0;
                                    inHand = new int[6];
                                    for (int j = 1; j <= 7; j++) {
                                        CardType = deck.DrawCard();
                                        if (CardType < 5) {
                                            LandsInHand++;
                                        }
                                        inHand[CardType]++;
                                    }
                                    Mulligan = LandsInHand < 2 || LandsInHand > 5;
                                    StartingHandSize--;
                                }
                                if (StartingHandSize < 7) {
                                    LandsInHand = Math.min(LandsInHand, StartingHandSize - 1);
                                    int remaining = LandsInHand - inHand[3];
                                    if (inHand[1] + inHand[2] > remaining) {
                                        int total = requirement.getColorRequirements()[0] + requirement.getColorRequirements()[1];
                                        inHand[1] = Math.min(inHand[1], remaining * requirement.getColorRequirements()[0] / total);
                                        inHand[2] = Math.min(inHand[2], remaining * requirement.getColorRequirements()[1] / total);
                                    }
                                }
                                //For turns 2 on, draw cards for the number of turns available
                                for (int turn = 2; turn <= requirement.getCmc(); turn++) {
                                    CardType = deck.DrawCard();
                                    if (CardType < 5) {
                                        LandsInHand++;
                                    }
                                    inHand[CardType]++;
                                }
                                int landsForA = Math.min(requirement.getColorRequirements()[0], inHand[1]);
                                int landsForB = Math.min(requirement.getColorRequirements()[1], inHand[2]);
                                int landsNeeded = requirement.getColorRequirements()[0] + requirement.getColorRequirements()[1];
                                if (inHand[3] >= landsNeeded - landsForA - landsForB && LandsInHand >= requirement.getCmc()) {
                                    CountOK++;
                                }
                                if (LandsInHand >= requirement.getCmc()) {
                                    CountConditional++;
                                }
                            }
                            System.out.println(requirement.getColorRequirements()[0] + "," + requirement.getColorRequirements()[1] + "," + requirement.getCmc() + "," + NrLandA + "," + NrLandB + "," + NrLandAB + "," + requirement.getNumberOfLands() + "," + requirement.getNumberOfCards() + "," + CountOK / CountConditional);
                        }
                    }
                }
            }
        }
    }
}

class Deck {
    int NumberOfLands;
    int[] mNumberOfGoodLands;
    int NumberOfCards;

    void SetDeck (int NrLands, int NrGoodLands, int NrCards) {
        NumberOfLands=NrLands;
        mNumberOfGoodLands = new int[] { NrGoodLands };
        NumberOfCards=NrCards;
    }

    void SetDeck (int NrLands, int[] NrGoodLands, int NrCards) {
        NumberOfLands = NrLands;
        mNumberOfGoodLands = NrGoodLands;
        NumberOfCards = NrCards;
    }

    int DrawCard (){
        Random generator = new Random();
        int RandomIntegerBetweenOneAndDeckSize=generator.nextInt( this.NumberOfCards)+1;
        int GoodLandCutoff = 0;
        for (int i=0; i < mNumberOfGoodLands.length; i++) {
            GoodLandCutoff += mNumberOfGoodLands[i];
            if (RandomIntegerBetweenOneAndDeckSize <= GoodLandCutoff) {
                mNumberOfGoodLands[i]--;
                NumberOfLands--;
                NumberOfCards--;
                return i + 1;
            }
        }
        if (RandomIntegerBetweenOneAndDeckSize <= NumberOfLands) {
            NumberOfCards--;
            NumberOfLands--;
            return mNumberOfGoodLands.length + 1;
        }
        else {
            NumberOfCards--;
            return mNumberOfGoodLands.length + 2;
        }
    }
}