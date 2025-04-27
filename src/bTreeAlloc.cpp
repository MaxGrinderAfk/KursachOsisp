#include "bTreeAlloc.h"

void printFrameTop() {
    int termWidth = getTerminalWidth();
    std::cout << CYAN << std::string(termWidth, '=') << RESET << std::endl;
}

void printFrameBottom() {
    int termWidth = getTerminalWidth();
    std::cout << CYAN << std::string(termWidth, '=') << RESET << std::endl;
}

void showProgressBar(int percent, bool isRunningAllTests) {
    int termWidth = getTerminalWidth();
    int width = termWidth * 0.6; 
    
    int pos = percent * width / 100;
    
    std::string progressBar = BOLDWHITE "[";
    
    for (int i = 0; i < width; ++i) {
        if (i < pos) progressBar += GREEN "=";
        else if (i == pos) progressBar += YELLOW ">";
        else progressBar += RESET " ";
    }
    
    progressBar += BOLDWHITE "] " + std::to_string(percent) + "%";
    
    int visibleLength = width + 6; 
    int padding = std::max(0, (termWidth - visibleLength) / 2);
    
    std::cout << "\r" << std::string(padding, ' ') << progressBar << RESET;
    std::cout.flush();
    
    if (percent == 100 && !isRunningAllTests) {
        std::cout << std::endl;
    }
}

int getTerminalWidth() {
    struct winsize w;
    ioctl(STDOUT_FILENO, TIOCGWINSZ, &w);
    return w.ws_col;
}

void printCentered(const std::string& text) {
    int termWidth = getTerminalWidth();
    std::string plainText = text;
    
    size_t colorPos = 0;
    int colorLen = 0;
    
    while (true) {
        colorPos = plainText.find("\033[", colorPos);
        if (colorPos == std::string::npos) break;
        
        size_t endPos = plainText.find('m', colorPos);
        if (endPos != std::string::npos) {
            int len = endPos - colorPos + 1;
            plainText.erase(colorPos, len);
            colorLen += len;
        } else {
            break;
        }
    }
    
    int textLen = text.length() - colorLen;
    int padding = (termWidth - textLen) / 2;
    
    if (padding > 0)
        std::cout << std::string(padding, ' ');
    
    std::cout << text << std::endl;
}

void enableRawMode(struct termios& original) {
    struct termios raw;
    tcgetattr(STDIN_FILENO, &original);
    raw = original;
    raw.c_lflag &= ~(ICANON | ECHO); 
    tcsetattr(STDIN_FILENO, TCSANOW, &raw);
}

void disableRawMode(struct termios& original) {
    tcsetattr(STDIN_FILENO, TCSANOW, &original);
}

void runSingleTest(void (*testFunc)(), const std::string& testName) {
    std::cout << "\033c";
    printFrameTop();
    printCentered(BOLDWHITE "=== Запуск теста: " + testName + " ===");
    printFrameBottom();
    
    if (testName == "Финальный стресс-тест" || testName == "Финальный стресс-тест (строки)") {
        for (float i = 0; i <= 100.0; i += 0.25) {  
            showProgressBar(i);
            std::this_thread::sleep_for(std::chrono::milliseconds(30));  
        }
    } else {
        for (int i = 0; i <= 100; i += 5) {
            showProgressBar(i);
            std::this_thread::sleep_for(std::chrono::milliseconds(30));
        }
    }
    
    testFunc();
    
    printCentered(GREEN "\nТест успешно пройден!" RESET);
}

void runAllTests() {
    std::cout << "\033c";
    printFrameTop();
    printCentered(BOLDWHITE "=== Запуск всех тестов ===");
    printFrameBottom();
    
    std::vector<std::pair<void(*)(), std::string>> tests = {
        {testBasicOperations, "Базовые операции"},
        {testEdgeCases, "Крайние случаи"},
        {testConcurrency, "Многопоточная вставка"},
        {testRepeatedInsertionsAndDeletions, "Повторные вставки и удаления"},
        {testLargeRangeInsertions, "Массовая вставка большого диапазона"},
        {testTreeSizeApproximation, "Проверка размера дерева"},
        {testAlternatingInsertRemove, "Перемешанные вставки и удаления"},
        {testConcurrencyMixed, "Смешанная многопоточность"},
        {testHeavyConcurrency, "Финальный стресс-тест"},
        {testHeavyConcurrencyString, "Финальный стресс-тест (строки)"}
    };
    
    for (size_t i = 0; i < tests.size(); ++i) {
        printCentered(BOLDWHITE "\nЗапуск теста " + std::to_string(i + 1) + "/" + 
                     std::to_string(tests.size()) + ": " + tests[i].second);
        
        if (i == 8 || i == 9) {  
            for (float i = 0; i <= 100.0; i += 0.25) { 
                showProgressBar(i);
                std::this_thread::sleep_for(std::chrono::milliseconds(30)); 
            }
        } else {
            for (int j = 0; j <= 100; j += 4) {
                showProgressBar(j, true);
                std::this_thread::sleep_for(std::chrono::milliseconds(20));
            }
        }
        
        tests[i].first();
        
        int overallProgress = (i + 1) * 100 / tests.size();
        printCentered(GREEN "✓ Тест пройден!" RESET);
        printCentered(BOLDCYAN "Общий прогресс: " + std::to_string(overallProgress) + "%" RESET);
    }
    
    std::cout << std::endl;
    printFrameTop();
    printCentered(GREEN "\nВсе тесты успешно пройдены!" RESET);
    printFrameBottom();
}

void showMenu() {
    std::cout << "\033c";
    printFrameTop();
    printCentered(BOLDWHITE "=== Меню тестов B-дерева ===");
    printFrameBottom();

    printCentered(GREEN "1. Базовые операции" RESET " — Вставка, поиск, удаление.");
    printCentered(GREEN "2. Крайние случаи" RESET " — Пустое дерево, большие вставки.");
    printCentered(GREEN "3. Многопоточная вставка" RESET " — Несколько потоков.");
    printCentered(GREEN "4. Повторные вставки и удаления" RESET " — Двойные операции.");
    printCentered(GREEN "5. Массовая вставка большого диапазона" RESET " — Много больших чисел.");
    printCentered(GREEN "6. Проверка размера дерева" RESET " — Проверка после вставки/удаления.");
    printCentered(GREEN "7. Перемешанные вставки и удаления" RESET " — Чередование операций.");
    printCentered(GREEN "8. Смешанная многопоточность" RESET " — Вставка и удаление вместе.");
    printCentered(GREEN "9. Финальный стресс-тест" RESET " — Серьезная нагрузка.");
    printCentered(GREEN "s. Финальный стресс-тест (строки)" RESET " — Серьезная нагрузка со строками.");
    printCentered(BOLDCYAN ": Запустить все тесты подряд" RESET);
    printCentered(RED "0. Выход" RESET);

    printFrameTop();
    printCentered(YELLOW "Нажмите цифру (0 - 9), s или : для запуска всех тестов:");
}

void testBasicOperations() {
    std::cout << "\n=== Тест 1: Базовые операции ===" << std::endl;
    BTree<int> tree(3);
    
    for (int i : {10, 20, 5, 15, 25, 30, 1, 2}) {
        tree.insert(i);
    }
    
    for (int i : {10, 20, 5, 15, 25, 30, 1, 2}) {
        assert(tree.search(i) && "Элемент не найден после вставки");
    }
    
    tree.remove(10);
    tree.remove(5);
    tree.remove(30);
    
    assert(!tree.search(10) && "Удаленный элемент найден");
    assert(!tree.search(5) && "Удаленный элемент найден");
    assert(!tree.search(30) && "Удаленный элемент найден");
    
    for (int i : {20, 15, 25, 1, 2}) {
        assert(tree.search(i) && "Оставшийся элемент не найден");
    }
    
    std::cout << "Тест 1 пройден успешно!\n";
}

void testEdgeCases() {
    std::cout << "\n=== Тест 2: Крайние случаи ===" << std::endl;
    BTree<int> tree(2);
    
    tree.insert(100);
    assert(tree.search(100) && "Единственный элемент не найден");
    
    tree.remove(100);
    assert(!tree.search(100) && "Удаленный элемент найден");
    
    tree.remove(42); 
    
    for (int i = 0; i < 1000; ++i) {
        tree.insert(i);
    }
    for (int i = 0; i < 1000; ++i) {
        assert(tree.search(i) && "Элемент не найден после массовой вставки");
    }
    for (int i = 0; i < 1000; ++i) {
        tree.remove(i);
    }
    for (int i = 0; i < 1000; ++i) {
        assert(!tree.search(i) && "Элемент найден после массового удаления");
    }
    
    std::cout << "Тест 2 пройден успешно!\n";
}

void testConcurrency() {
    std::cout << "\n=== Тест 3: Многопоточные операции ===" << std::endl;
    BTree<int> tree(3);
    const int numThreads = 4;
    const int numElements = 1000;
    
    auto insertFunc = [&tree](int start) {
        for (int i = start; i < start + numElements; ++i) {
            tree.insert(i);
        }
    };
    
    std::vector<std::thread> threads;
    for (int i = 0; i < numThreads; ++i) {
        threads.emplace_back(insertFunc, i * numElements);
    }
    for (auto& t : threads) {
        t.join();
    }
    
    for (int i = 0; i < numThreads * numElements; ++i) {
        assert(tree.search(i) && "Элемент не найден после многопоточной вставки");
    }
    
    threads.clear();
    auto removeFunc = [&tree](int start) {
        for (int i = start; i < start + numElements; ++i) {
            tree.remove(i);
        }
    };
    
    for (int i = 0; i < numThreads; ++i) {
        threads.emplace_back(removeFunc, i * numElements);
    }
    for (auto& t : threads) {
        t.join();
    }
    
    for (int i = 0; i < numThreads * numElements; ++i) {
        assert(!tree.search(i) && "Элемент найден после многопоточного удаления");
    }
    
    std::cout << "Тест 3 пройден успешно!\n";
}

void testRepeatedInsertionsAndDeletions() {
    std::cout << "\n=== Тест 4: Повторные вставки и удаления ===" << std::endl;
    BTree<int> tree(3);

    for (int i = 0; i < 100; ++i) {
        tree.insert(i);
        tree.insert(i); 
    }

    for (int i = 0; i < 100; ++i) {
        assert(tree.search(i) && "Элемент не найден после повторной вставки");
    }

    for (int i = 0; i < 100; ++i) {
        tree.remove(i);
        tree.remove(i);
    }

    for (int i = 0; i < 100; ++i) {
        assert(!tree.search(i) && "Элемент найден после двойного удаления");
    }

    std::cout << "Тест 4 пройден успешно!\n";
}

void testLargeRangeInsertions() {
    std::cout << "\n=== Тест 5: Массовая вставка большого диапазона значений ===" << std::endl;
    BTree<int> tree(5);

    for (int i = 1000000; i < 1001000; ++i) {
        tree.insert(i);
    }

    for (int i = 1000000; i < 1001000; ++i) {
        assert(tree.search(i) && "Элемент не найден после вставки большого диапазона");
    }

    std::cout << "Тест 5 пройден успешно!\n";
}

void testTreeSizeApproximation() {
    std::cout << "\n=== Тест 6: Приближенная проверка размера дерева ===" << std::endl;
    BTree<int> tree(3);

    for (int i = 0; i < 500; ++i) {
        tree.insert(i);
    }

    for (int i = 250; i < 500; ++i) {
        tree.remove(i);
    }

    for (int i = 0; i < 250; ++i) {
        assert(tree.search(i) && "Элемент должен быть в дереве");
    }

    for (int i = 250; i < 500; ++i) {
        assert(!tree.search(i) && "Элемент не должен быть в дереве");
    }

    std::cout << "Тест 6 пройден успешно!\n";
}

void testAlternatingInsertRemove() {
    std::cout << "\n=== Тест 7: Перемешанные вставки и удаления ===" << std::endl;
    BTree<int> tree(4);

    for (int i = 0; i < 10000; i += 2) {
        tree.insert(i);
    }

    for (int i = 0; i < 10000; ++i) {
        if (i % 4 == 0) {
            tree.remove(i);
        } else if (i % 2 == 0) {
            assert(tree.search(i) && "Элемент должен быть найден после вставки");
        }
    }

    for (int i = 0; i < 10000; ++i) {
        if (i % 4 == 0) {
            assert(!tree.search(i) && "Элемент не должен быть найден после удаления");
        }
    }

    std::cout << "Тест 7 пройден успешно!\n";
}

void testConcurrencyMixed() {
    std::cout << "\n=== Тест 8: Смешанная многопоточность (вставка и удаление одновременно) ===" << std::endl;

    BTree<int> tree(3);
    const int numThreads = 4;
    const int numElements = 1000;

    auto insertFunc = [&tree](int start) {
        for (int i = start; i < start + numElements; ++i) {
            tree.insert(i);
        }
    };

    auto removeFunc = [&tree](int start) {
        for (int i = start; i < start + numElements; ++i) {
            tree.remove(i);
        }
    };

    std::vector<std::thread> threads;

    for (int i = 0; i < numThreads; ++i) {
        if (i % 2 == 0) {
            threads.emplace_back(insertFunc, i * numElements);
        } else {
            threads.emplace_back(removeFunc, (i - 1) * numElements); 
        }
    }

    for (auto& t : threads) {
        t.join();
    }

    int insertedElements = (numThreads / 2) * numElements;
    int foundElements = 0;
    for (int i = 0; i < insertedElements * 2; ++i) {
        bool found = tree.search(i);
        if (found) {
            foundElements++;
            if (i % 2 == 1) {
                assert(found && "Элемент должен быть найден после вставки нечетного потоком");
            }
        } else {
            if (i % 2 == 0 && i < insertedElements) {
                assert(!found && "Элемент не должен быть найден после удаления четного потоком");
            }
        }
    }
    
    std::cout << "Найдено элементов: " << foundElements << std::endl;
    std::cout << "Тест 8 пройден успешно!\n";
}

void testHeavyConcurrency() {
    std::cout << "\n=== Тест 9: Финальный стресс-тест ===" << std::endl;
    BTree<int> tree(4);
    const int numThreads = 16;
    const int operationsPerThread = 50000;
    const int keyRange = 100000;

    std::atomic<bool> running{true};
    std::atomic<int> insertCount{0};
    std::atomic<int> removeCount{0};

    std::vector<std::thread> threads;
    std::atomic<int>* keyCounters = new std::atomic<int>[keyRange + 1]();

    auto worker = [&](int id) {
        std::mt19937 rng(std::random_device{}() + id);
        std::uniform_int_distribution<int> opDist(0, 1);
        std::uniform_int_distribution<int> keyDist(0, keyRange);
    
        for (int i = 0; i < operationsPerThread; ++i) {
            int op = opDist(rng);
            int key = keyDist(rng);
    
            if (op == 0) {
                tree.insert(key);
                keyCounters[key].fetch_add(1, std::memory_order_relaxed);
                insertCount.fetch_add(1, std::memory_order_relaxed);
            } else {
                int expected = keyCounters[key].load(std::memory_order_relaxed);
                if (expected > 0) {
                    bool success = false;
                    while (expected > 0 && !success) {
                        success = keyCounters[key].compare_exchange_weak(
                            expected, expected - 1,
                            std::memory_order_release,
                            std::memory_order_relaxed);
                        if (success) {
                            tree.remove(key);
                            removeCount.fetch_add(1, std::memory_order_relaxed);
                        } else {
                            expected = keyCounters[key].load(std::memory_order_relaxed);
                        }
                    }
                }
            }
        }
    };

    for (int i = 0; i < numThreads; ++i) {
        threads.emplace_back(worker, i);
    }

    for (auto& t : threads) {
        t.join();
    }

    std::cout << "Все потоки завершены. Проверка целостности дерева..." << std::endl;
    std::cout << "Вставлено ключей: " << insertCount.load() << std::endl;
    std::cout << "Удалено ключей: " << removeCount.load() << std::endl;

    std::unordered_set<int> finalExpectedKeys;
    for (int key = 0; key <= keyRange; ++key) {
        int count = keyCounters[key].load(std::memory_order_relaxed);
        if (count > 0) {
            finalExpectedKeys.insert(key);
        }
    }

    int foundCount = 0;
    int discrepancies = 0;
    std::vector<int> missingKeys;
    std::vector<int> extraKeys;

    for (int key = 0; key <= keyRange; ++key) {
        bool expected = finalExpectedKeys.count(key) > 0;
        bool actual = tree.search(key);

        if (expected != actual) {
            if (discrepancies < 10) {
                std::cerr << "Mismatch for key " << key << ": Expected="
                          << expected << ", Actual=" << actual 
                          << ", Counter=" << keyCounters[key].load() << std::endl;
                if (expected && !actual) missingKeys.push_back(key);
                if (!expected && actual) extraKeys.push_back(key);
            }
            discrepancies++;
        }

        if (actual) {
            foundCount++;
        }
    }

    if (discrepancies > 0) {
        std::cerr << "ОШИБКА: Обнаружено " << discrepancies << " несоответствий!" << std::endl;
        if (!missingKeys.empty()) {
            std::cerr << "Примеры отсутствующих ключей: ";
            for (size_t i = 0; i < std::min(missingKeys.size(), (size_t)5); ++i) {
                std::cerr << missingKeys[i] << " ";
            }
            std::cerr << std::endl;
        }
        if (!extraKeys.empty()) {
            std::cerr << "Примеры лишних ключей: ";
            for (size_t i = 0; i < std::min(extraKeys.size(), (size_t)5); ++i) {
                std::cerr << extraKeys[i] << " ";
            }
            std::cerr << std::endl;
        }
        assert(discrepancies == 0 && "Несоответствие наличия ключа после стресс-теста!");
    } else {
        std::cout << "Проверка целостности дерева прошла успешно: несоответствий не обнаружено!" << std::endl;
        std::cout << "Финальный стресс-тест успешно пройден!" << std::endl;
    }
    std::cout << "Найдено элементов в дереве: " << foundCount << "\n";

    delete[] keyCounters;
}

void testHeavyConcurrencyString() {
    std::cout << "\n=== Тест 10: Финальный стресс-тест(строки) ===" << std::endl;
    BTree<std::string> tree(4);
    const int numThreads = 16;
    const int operationsPerThread = 50000;
    const int keyRange = 100000;

    std::atomic<bool> running{true};
    std::atomic<int> insertCount{0};
    std::atomic<int> removeCount{0};

    std::vector<std::thread> threads;
    std::atomic<int>* keyCounters = new std::atomic<int>[keyRange + 1]();

    auto worker = [&](int id) {
        std::mt19937 rng(std::random_device{}() + id);
        std::uniform_int_distribution<int> opDist(0, 1);
        std::uniform_int_distribution<int> keyDist(0, keyRange);
    
        for (int i = 0; i < operationsPerThread; ++i) {
            int op = opDist(rng);
            std::string key = "key_" + std::to_string(keyDist(rng));
            int keyIndex = std::stoi(key.substr(4));
    
            if (op == 0) {
                tree.insert(key);
                keyCounters[keyIndex].fetch_add(1, std::memory_order_relaxed);
                insertCount.fetch_add(1, std::memory_order_relaxed);
            } else {
                int expected = keyCounters[keyIndex].load(std::memory_order_relaxed);
                if (expected > 0) {
                    bool success = false;
                    while (expected > 0 && !success) {
                        success = keyCounters[keyIndex].compare_exchange_weak(
                            expected, expected - 1,
                            std::memory_order_release,
                            std::memory_order_relaxed);
                        if (success) {
                            tree.remove(key);
                            removeCount.fetch_add(1, std::memory_order_relaxed);
                        } else {
                            expected = keyCounters[keyIndex].load(std::memory_order_relaxed);
                        }
                    }
                }
            }
        }
    };

    for (int i = 0; i < numThreads; ++i) {
        threads.emplace_back(worker, i);
    }

    for (auto& t : threads) {
        t.join();
    }

    std::cout << "Все потоки завершены. Проверка целостности дерева..." << std::endl;
    std::cout << "Вставлено ключей: " << insertCount.load() << std::endl;
    std::cout << "Удалено ключей: " << removeCount.load() << std::endl;

    std::unordered_set<int> finalExpectedKeys;
    for (int key = 0; key <= keyRange; ++key) {
        int count = keyCounters[key].load(std::memory_order_relaxed);
        if (count > 0) {
            finalExpectedKeys.insert(key);
        }
    }

    int foundCount = 0;
    int discrepancies = 0;
    std::vector<std::string> missingKeys;
    std::vector<std::string> extraKeys;

    for (int key = 0; key <= keyRange; ++key) {
        std::string keyStr = "key_" + std::to_string(key);
        bool expected = finalExpectedKeys.count(key) > 0;
        bool actual = tree.search(keyStr);

        if (expected != actual) {
            if (discrepancies < 10) {
                std::cerr << "Mismatch for key " << keyStr << ": Expected="
                          << expected << ", Actual=" << actual 
                          << ", Counter=" << keyCounters[key].load() << std::endl;
                if (expected && !actual) missingKeys.push_back(keyStr);
                if (!expected && actual) extraKeys.push_back(keyStr);
            }
            discrepancies++;
        }

        if (actual) {
            foundCount++;
        }
    }

    if (discrepancies > 0) {
        std::cerr << "ОШИБКА: Обнаружено " << discrepancies << " несоответствий!" << std::endl;
        if (!missingKeys.empty()) {
            std::cerr << "Примеры отсутствующих ключей: ";
            for (size_t i = 0; i < std::min(missingKeys.size(), (size_t)5); ++i) {
                std::cerr << missingKeys[i] << " ";
            }
            std::cerr << std::endl;
        }
        if (!extraKeys.empty()) {
            std::cerr << "Примеры лишних ключей: ";
            for (size_t i = 0; i < std::min(extraKeys.size(), (size_t)5); ++i) {
                std::cerr << extraKeys[i] << " ";
            }
            std::cerr << std::endl;
        }
        assert(discrepancies == 0 && "Несоответствие наличия ключа после стресс-теста!");
    } else {
        std::cout << "Проверка целостности дерева прошла успешно: несоответствий не обнаружено!" << std::endl;
        std::cout << "Финальный стресс-тест успешно пройден!" << std::endl;
    }
    std::cout << "Найдено элементов в дереве: " << foundCount << "\n";

    delete[] keyCounters;
}

void runApplication() {
    struct termios original;
    enableRawMode(original);

    while (true) {
        showMenu();
        
        char choice = getchar(); 
        std::cout << "\n";

        switch (choice) {
            case '1': runSingleTest(testBasicOperations, "Базовые операции"); break;
            case '2': runSingleTest(testEdgeCases, "Крайние случаи"); break;
            case '3': runSingleTest(testConcurrency, "Многопоточная вставка"); break;
            case '4': runSingleTest(testRepeatedInsertionsAndDeletions, "Повторные вставки и удаления"); break;
            case '5': runSingleTest(testLargeRangeInsertions, "Массовая вставка большого диапазона"); break;
            case '6': runSingleTest(testTreeSizeApproximation, "Проверка размера дерева"); break;
            case '7': runSingleTest(testAlternatingInsertRemove, "Перемешанные вставки и удаления"); break;
            case '8': runSingleTest(testConcurrencyMixed, "Смешанная многопоточность"); break;
            case '9': runSingleTest(testHeavyConcurrency, "Финальный стресс-тест"); break;
            case 's': 
            case 'S': runSingleTest(testHeavyConcurrencyString, "Финальный стресс-тест (строки)"); break;
            case ':': 
                runAllTests(); break;
            case '0':
                disableRawMode(original);
                printCentered(RED "Выход из программы. До свидания!" RESET);
                return;
            default:
                printCentered(RED "Некорректный выбор! Пожалуйста, нажмите цифру от 0 до 9, s или : для прохождения всех тестов сразу." RESET);
                break;
        }

        printCentered(YELLOW "\nНажмите любую клавишу для возврата в меню..." RESET);
        getchar(); 
    }
}