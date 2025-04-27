#ifndef BTREE_ALLOC_H
#define BTREE_ALLOC_H

#include "libs.h"
#include "bTree.h"

void testBasicOperations();
void testEdgeCases();
void testConcurrency();
void testRepeatedInsertionsAndDeletions();
void testLargeRangeInsertions();
void testTreeSizeApproximation();
void testAlternatingInsertRemove();
void testConcurrencyMixed();
void testHeavyConcurrency();
void testHeavyConcurrencyString();
void showMenu();
void runAllTests();
int getTerminalWidth();
void printCentered(const std::string& text);
void enableRawMode(struct termios& original);
void disableRawMode(struct termios& original);
void printFrameTop();
void printFrameBottom();
void showProgressBar(int percent, bool isRunningAllTests = false);
void runSingleTest(void (*testFunc)(), const std::string& testName);
void runApplication();

#endif