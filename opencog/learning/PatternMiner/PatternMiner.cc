/*
 * opencog/learning/PatternMiner/PatternMiner.h
 *
 * Copyright (C) 2012 by OpenCog Foundation
 * All Rights Reserved
 *
 * Written by Shujing Ke <rainkekekeke@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License v3 as
 * published by the Free Software Foundation and including the exceptions
 * at http://opencog.org/wiki/Licenses
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program; if not, write to:
 * Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */
#include <thread>
#include <sstream>
#include <iostream>
#include <fstream>
#include <map>
#include <iterator>
#include <opencog/atomspace/atom_types.h>
#include <opencog/util/StringManipulator.h>
#include <opencog/util/foreach.h>
#include <opencog/query/PatternMatch.h>
#include <stdlib.h>
#include <opencog/atomspace/Handle.h>
#include "PatternMiner.h"

using namespace opencog::PatternMining;
using namespace opencog;

void PatternMiner::generateIndexesOfSharedVars(Handle& link, vector<Handle>& orderedHandles, vector < vector<int> >& indexes)
{
    HandleSeq outgoingLinks = atomSpace->getOutgoing(link);
    foreach (Handle h, outgoingLinks)
    {
        if (atomSpace->isNode(h))
        {
            if (atomSpace->getType(h) == opencog::VARIABLE_NODE)
            {
                string var_name = atomSpace->getName(h);

                vector<int> indexesForCurVar;
                int index = 0;

                foreach (Handle oh,orderedHandles)
                {
                    string ohStr = atomSpace->atomAsString(oh);
                    if (ohStr.find(var_name) != std::string::npos)
                    {
                        indexesForCurVar.push_back(index);
                    }

                    index ++;
                }

                indexes.push_back(indexesForCurVar);
            }
        }
        else
            generateIndexesOfSharedVars(h,orderedHandles,indexes);
    }
}

void PatternMiner::findAndRenameVariablesForOneLink(Handle link, map<Handle,Handle>& varNameMap, HandleSeq& renameOutgoingLinks)
{

    HandleSeq outgoingLinks = atomSpace->getOutgoing(link);

    foreach (Handle h, outgoingLinks)
    {
        if (atomSpace->isNode(h))
        {
           if (atomSpace->getType(h) == opencog::VARIABLE_NODE)
           {
               if (varNameMap.find(h) != varNameMap.end())
               {
                   renameOutgoingLinks.push_back(varNameMap[h]);
               }
               else
               {
                   string var_name = "$var_"  + toString(varNameMap.size() + 1);
                   Handle var_node = atomSpace->addNode(opencog::VARIABLE_NODE, var_name, TruthValue::TRUE_TV());
                   varNameMap.insert(std::pair<Handle,Handle>(h,var_node));
                   renameOutgoingLinks.push_back(var_node);
               }
           }
        }
        else
        {
             HandleSeq _renameOutgoingLinks;
             findAndRenameVariablesForOneLink(h, varNameMap, _renameOutgoingLinks);
             Handle reLink = atomSpace->addLink(atomSpace->getType(h),_renameOutgoingLinks,TruthValue::TRUE_TV());
             renameOutgoingLinks.push_back(reLink);
        }

    }

}

vector<Handle> PatternMiner::RebindVariableNames(vector<Handle>& orderedPattern, map<Handle,Handle>& orderedVarNameMap)
{

    vector<Handle> rebindedPattern;

    foreach (Handle link, orderedPattern)
    {
        HandleSeq renameOutgoingLinks;
        findAndRenameVariablesForOneLink(link, orderedVarNameMap, renameOutgoingLinks);
        Handle rebindedLink = atomSpace->addLink(atomSpace->getType(link),renameOutgoingLinks,TruthValue::TRUE_TV());
        rebindedPattern.push_back(rebindedLink);
    }

    return rebindedPattern;
}

// the input links should be like: only specify the const node, all the variable node name should not be specified:
vector<Handle> PatternMiner::UnifyPatternOrder(vector<Handle>& inputPattern)
{

    // Step 1: take away all the variable names, make the pattern into such format string:
    //    (InheritanceLink
    //       (VariableNode )
    //       (ConceptNode "Animal")

    //    (InheritanceLink
    //       (VariableNode )
    //       (VariableNode )

    //    (InheritanceLink
    //       (VariableNode )
    //       (VariableNode )

    //    (EvaluationLink (stv 1 1)
    //       (PredicateNode "like_food")
    //       (ListLink
    //          (VariableNode )
    //          (ConceptNode "meat")
    //       )
    //    )

    multimap<string, Handle> nonVarStrToHandleMap;

    foreach (Handle inputH, inputPattern)
    {
        string str = atomSpace->atomAsString(inputH);
        string nonVarNameString = "";
        std::stringstream stream(str);
        string oneLine;

        while(std::getline(stream, oneLine,'\n'))
        {
            if (oneLine.find("VariableNode")==std::string::npos)
            {
                // this node is not a VariableNode, just keep this line
                nonVarNameString += oneLine;
            }
            else
            {
                // this node is an VariableNode, remove the name, just keep "VariableNode"
                nonVarNameString += "VariableNode\n";
            }
        }

        nonVarStrToHandleMap.insert(std::pair<string, Handle>(nonVarNameString,inputH));
    }

    // Step 2: sort the order of all the handls do not share the same string key with other handls
    // becasue the strings are put into a map , so they are already sorted.
    // now print the Handles that do not share the same key with other Handles into a vector, left the Handles share the same keys

    vector<Handle> orderedHandles;
    vector<string> duplicateStrs;
    multimap<string, Handle>::iterator it;
    for(it = nonVarStrToHandleMap.begin(); it != nonVarStrToHandleMap.end();)
    {
        int count = nonVarStrToHandleMap.count(it->first);
        if (count == 1)
        {
            // if this key string has only one record , just put the corresponding handle to the end of orderedHandles
            orderedHandles.push_back(it->second);
            it ++;
        }
        else
        {
            // this key string has multiple handles to it, not put these handles into the orderedHandles,
            // insteadly put this key string into duplicateStrs
            duplicateStrs.push_back(it->first);
            it = nonVarStrToHandleMap.upper_bound(it->first);
        }

    }

    // Step 3: sort the order of the handls share the same string key with other handles
    foreach (string keyString, duplicateStrs)
    {
        // get all the corresponding handles for this key string
        multimap<string, Handle>::iterator kit;
        vector<_non_ordered_pattern> sharedSameKeyPatterns;
        for (kit = nonVarStrToHandleMap.lower_bound(keyString); kit != nonVarStrToHandleMap.upper_bound(keyString);  ++ kit)
        {
            _non_ordered_pattern p;
            p.link = kit->second;
            generateIndexesOfSharedVars(p.link, orderedHandles, p.indexesOfSharedVars);
            sharedSameKeyPatterns.push_back(p);
        }

        std::sort(sharedSameKeyPatterns.begin(), sharedSameKeyPatterns.end());
        foreach (_non_ordered_pattern np, sharedSameKeyPatterns)
        {
            orderedHandles.push_back(np.link);
        }
    }

    // in this map, the first Handle is the variable node is the original Atomspace,
    // the second Handle is the renamed ordered variable node in the Pattern Mining Atomspace.
    map<Handle,Handle> orderedVarNameMap;
    vector<Handle> rebindPattern = RebindVariableNames(orderedHandles, orderedVarNameMap);

    return rebindPattern;

}

string PatternMiner::unifiedPatternToKeyString(vector<Handle>& inputPattern)
{
    string keyStr = "";
    foreach(Handle h, inputPattern)
    {
        keyStr += atomSpace->atomAsString(h);
        keyStr += "\n";
    }

    return keyStr;
}

bool PatternMiner::checkPatternExist(const string& patternKeyStr)
{
    if (keyStrToHTreeNodeMap.find(patternKeyStr) == keyStrToHTreeNodeMap.end())
        return false;
    else
        return true;

}

void generateNextCombinationGroup(bool* &indexes, int n_max)
{
    int trueCount = -1;
    int i = 0;
    for (; i < n_max - 1; ++ i)
    {
        if (indexes[i])
        {
            ++ trueCount;

            if (! indexes[i+1])
                break;
        }
    }

    indexes[i] = false;
    indexes[i+1] = true;

    for (int j = 0; j < trueCount; ++ j)
        indexes[j] = true;

    for (int j = trueCount; j < i; ++ j)
        indexes[j] = false;

}

bool isLastNElementsAllTrue(bool* array, int size, int n)
{
    for (int i = size - 1; i >= size - n; i --)
    {
        if (! array[i])
            return false;
    }

    return true;
}

void PatternMiner::generateALinkByChosenVariables(Handle& originalLink, map<Handle,Handle>& valueToVarMap,  HandleSeq& outputOutgoings)
{
    HandleSeq outgoingLinks = originalAtomSpace->getOutgoing(originalLink);

    foreach (Handle h, outgoingLinks)
    {
        if (originalAtomSpace->isNode(h))
        {
           if (valueToVarMap.find(h) != valueToVarMap.end())
           {
               // this node is considered as a variable
               outputOutgoings.push_back(valueToVarMap[h]);
           }
           else
           {
               // this node is considered not a variable, so add its bound value node into the Pattern mining Atomspace
               Handle value_node = atomSpace->addNode(originalAtomSpace->getType(h), originalAtomSpace->getName(h), TruthValue::TRUE_TV());
               outputOutgoings.push_back(value_node);
           }
        }
        else
        {
             HandleSeq _outputOutgoings;
             generateALinkByChosenVariables(h, valueToVarMap, _outputOutgoings);
             Handle reLink = atomSpace->addLink(originalAtomSpace->getType(h),_outputOutgoings,TruthValue::TRUE_TV());
             outputOutgoings.push_back(reLink);
        }
    }
}

void PatternMiner::extractAllNodesInLink(Handle link, map<Handle,Handle>& valueToVarMap)
{
    HandleSeq outgoingLinks = originalAtomSpace->getOutgoing(link);

    foreach (Handle h, outgoingLinks)
    {
        if (originalAtomSpace->isNode(h))
        {
            if (valueToVarMap.find(h) == valueToVarMap.end())
            {
                // add a variable node in Pattern miner Atomspace
                Handle varHandle = atomSpace->addNode(opencog::VARIABLE_NODE,"$var~" + toString(valueToVarMap.size()) );
                valueToVarMap.insert(std::pair<Handle,Handle>(h,varHandle));
            }
            else
            {
                extractAllNodesInLink(h,valueToVarMap);
            }

        }
    }
}

// Extract all possible patterns from the original Atomspace input links (full Combination), and add to the patternmining Atomspace
// Patterns are in the following format:
//    (InheritanceLink
//       (VariableNode )
//       (ConceptNode "Animal")

//    (InheritanceLink
//       (VariableNode )
//       (VariableNode )

//    (InheritanceLink
//       (VariableNode )
//       (VariableNode )

//    (EvaluationLink (stv 1 1)
//       (PredicateNode "like_food")
//       (ListLink
//          (VariableNode )
//          (ConceptNode "meat")
//       )
//    )
vector<HTreeNode*> PatternMiner::extractAllPossiblePatternsFromInputLinks(vector<Handle>& inputLinks, unsigned int gram)
{
    map<Handle,Handle> valueToVarMap;
    vector<HTreeNode*> allNewPatternKeys;

    // First, extract all the nodes in the input links
    foreach (Handle link, inputLinks)
        extractAllNodesInLink(link, valueToVarMap);

    int n_max = valueToVarMap.size();
    bool* indexes = new bool[n_max];

    OC_ASSERT( (n_max > 1),
              "PatternMiner::extractAllPossiblePatternsFromInputLinks: this group of links only has one node: %s!\n",
               atomSpace->atomAsString(inputLinks[0]).c_str() );

    // Generate all the possible combinations of all the nodes: all patterns including 1 ~ (valueToVarMap.size() - 1) variables
    for (int var_num = 1; var_num < n_max; ++ var_num)
    {
        // Use the binary method to generate all combinations:

        // generate the first combination
        for (int i = 0; i < var_num; ++ i)
            indexes[i] = true;

        for (int i = var_num; i <n_max; ++ i)
            indexes[i] = false;

        while (true)
        {
            // construct the pattern for this combination in the PatternMining Atomspace
            // generate the valueToVarMap for this pattern of this combination
            map<Handle,Handle>::iterator iter;
            map<Handle,Handle> patternVarMap;
            unsigned int index = 0;
            for (iter = valueToVarMap.begin(); iter != valueToVarMap.end(); ++ iter)
            {
                if (indexes[index])
                    patternVarMap.insert(std::pair<Handle,Handle>(iter->first, iter->second));

                index ++;
            }

            HandleSeq pattern, unifiedPattern;

            foreach (Handle link, inputLinks)
            {
                HandleSeq outgoingLinks;
                generateALinkByChosenVariables(link, patternVarMap, outgoingLinks);
                Handle rebindedLink = atomSpace->addLink(atomSpace->getType(link),outgoingLinks,TruthValue::TRUE_TV());
                pattern.push_back(rebindedLink);
            }

            // unify the pattern
            unifiedPattern = UnifyPatternOrder(pattern);

            string keyString = unifiedPatternToKeyString(unifiedPattern);

            // next, check if this pattern already exist (need lock)
            HTreeNode* newHTreeNode = 0;
            uniqueKeyLock.lock();

            if (keyStrToHTreeNodeMap.find(keyString) == keyStrToHTreeNodeMap.end())
            {
                newHTreeNode = new HTreeNode();
                keyStrToHTreeNodeMap.insert(std::pair<string, HTreeNode*>(keyString, newHTreeNode));
            }

            uniqueKeyLock.unlock();

            if (newHTreeNode)
            {
                newHTreeNode->pattern = unifiedPattern;
                allNewPatternKeys.push_back(newHTreeNode);
                newHTreeNode->patternVarMap = patternVarMap;
                (patternsForGram[gram-1]).push_back(newHTreeNode);
            }

            if (isLastNElementsAllTrue(indexes, n_max, var_num))
                break;

            // generate the next combination
            generateNextCombinationGroup(indexes, n_max);
        }
    }

    return allNewPatternKeys;

}

void PatternMiner::swapOneLinkBetweenTwoAtomSpace(AtomSpace* fromAtomSpace, AtomSpace* toAtomSpace, Handle& fromLink, HandleSeq& outgoings, HandleSeq &outVariableNodes)
{
    HandleSeq outgoingLinks = fromAtomSpace->getOutgoing(fromLink);

    foreach (Handle h, outgoingLinks)
    {
        if (fromAtomSpace->isNode(h))
        {
           Handle new_node = toAtomSpace->addNode(fromAtomSpace->getType(h), fromAtomSpace->getName(h), fromAtomSpace->getTV(h));
           outgoings.push_back(new_node);
           if (fromAtomSpace->getType(h) == VARIABLE_NODE)
               outVariableNodes.push_back(new_node);
        }
        else
        {
             HandleSeq _OutgoingLinks;
             swapOneLinkBetweenTwoAtomSpace(fromAtomSpace, toAtomSpace, h, _OutgoingLinks, outVariableNodes);
             Handle _link = toAtomSpace->addLink(fromAtomSpace->getType(h),_OutgoingLinks,fromAtomSpace->getTV(h));
             outgoings.push_back(_link);
        }
    }
}

HandleSeq PatternMiner::swapLinksBetweenTwoAtomSpace(AtomSpace* fromAtomSpace, AtomSpace* toAtomSpace, HandleSeq& fromLinks, HandleSeq& outVariableNodes)
{
    HandleSeq outPutLinks;

    foreach (Handle link, fromLinks)
    {
        HandleSeq outgoingLinks;
        swapOneLinkBetweenTwoAtomSpace(fromAtomSpace, toAtomSpace, link, outgoingLinks, outVariableNodes);
        Handle toLink = toAtomSpace->addLink(fromAtomSpace->getType(link),outgoingLinks,fromAtomSpace->getTV(link));
        outPutLinks.push_back(toLink);
    }

    return outPutLinks;
}

 // using PatternMatcher
void PatternMiner::findAllInstancesForGivenPattern(HTreeNode* HNode)
{
//     First, generate the Bindlink for using PatternMatcher to find all the instances for this pattern in the original Atomspace
//    (BindLink
//        ;; The variables to be bound
//        (Listlink)
//          (VariableNode "$var_1")
//          (VariableNode "$var_2")
//          ...
//        (ImplicationLink
//          ;; The pattern to be searched for
//          (pattern)
//          ;; The instance to be returned.
//          (result)
//        )
//     )

    HandleSeq variableNodes, implicationLinkOutgoings, bindLinkOutgoings;

    HandleSeq patternToMatch = swapLinksBetweenTwoAtomSpace(atomSpace, originalAtomSpace, HNode->pattern, variableNodes);

    if (HNode->pattern.size() == 1) // this pattern only contains one link
    {
        implicationLinkOutgoings.push_back(patternToMatch[0]); // the pattern to match
        implicationLinkOutgoings.push_back(patternToMatch[0]); // the results to return
    }
    else
    {
        Handle hAndLink = originalAtomSpace->addLink(AND_LINK, patternToMatch, TruthValue::TRUE_TV());
        implicationLinkOutgoings.push_back(hAndLink); // the pattern to match
        implicationLinkOutgoings.push_back(hAndLink); // the results to return
    }

    Handle hImplicationLink = originalAtomSpace->addLink(IMPLICATION_LINK, implicationLinkOutgoings, TruthValue::TRUE_TV());

    // add variable atoms
    Handle hVariablesListLink = originalAtomSpace->addLink(LIST_LINK, variableNodes, TruthValue::TRUE_TV());

    bindLinkOutgoings.push_back(hVariablesListLink);
    bindLinkOutgoings.push_back(hImplicationLink);
    Handle hBindLink = originalAtomSpace->addLink(BIND_LINK, bindLinkOutgoings, TruthValue::TRUE_TV());

    // Run pattern matcher
    PatternMatch pm;
    pm.set_atomspace(originalAtomSpace);

    Handle hResultListLink = pm.bindlink(hBindLink);

    std::cout<<"Debug: PatternMiner::findAllInstancesForGivenPattern: pattern matching results: " << std::endl
            << originalAtomSpace->atomAsString(hResultListLink).c_str() <<std::endl;

    // Get result
    // Note: Don't forget remove the hResultListLink and BindLink
    HandleSeq resultSet = originalAtomSpace->getOutgoing(hResultListLink);
    originalAtomSpace->removeAtom(hResultListLink);
    originalAtomSpace->removeAtom(hBindLink);

    foreach (Handle listH , resultSet)
    {
        HNode->instances.push_back(originalAtomSpace->getOutgoing(listH));
    }

}

void PatternMiner::growTheFirstGramPatternsTask()
{

    while (true)
    {
        allAtomListLock.lock();
        if (allLinks.size() <= 0)
            break;

        Handle cur_link = allLinks[allLinks.size() - 1];
        // if this link is listlink, ignore it
        if (originalAtomSpace->getType(cur_link) == opencog::LIST_LINK)
            continue;

        allLinks.pop_back();
        allAtomListLock.unlock();

        HandleSeq originalLinks;
        originalLinks.push_back(cur_link);

        // Extract all the possible patterns from this originalLinks, not duplicating the already existing patterns
        vector<HTreeNode*> newPatternNodes = extractAllPossiblePatternsFromInputLinks(originalLinks);

        foreach (HTreeNode* HNode, newPatternNodes)
        {
            HNode->parentLinks.push_back(htree->rootNode);

            // Find All Instances in the original AtomSpace For this Pattern
            findAllInstancesForGivenPattern(HNode);
        }

    }

}

bool PatternMiner::isInHandleSeq(Handle handle, HandleSeq &handles)
{
    foreach(Handle h, handles)
    {
        if (handle == h)
            return true;
    }

    return false;
}

bool PatternMiner::isIgnoredType(Type type)
{
    foreach (Type t, ignoredTypes)
    {
        if (t == type)
            return true;
    }

    return false;
}

Handle PatternMiner::getFirstNonIgnoredIncomingLink(AtomSpace *atomspace, Handle& handle)
{
    Handle cur_h = handle;
    while(true)
    {
        HandleSeq incomings = atomspace->getIncoming(cur_h);
        if (incomings.size() == 0)
            return Handle::UNDEFINED;

        if (isIgnoredType (atomspace->getType(incomings[0])))
        {
            cur_h = incomings[0];
            continue;
        }
        else
            return incomings[0];

    }

}

void PatternMiner::extendAllPossiblePatternsForOneMoreGram(HandleSeq &instance, HTreeNode* curHTreeNode, unsigned int gram)
{
    map<Handle, Handle>::iterator varIt = curHTreeNode->patternVarMap.begin();

    for(;varIt != curHTreeNode->patternVarMap.end(); ++ varIt)
    {
        // find what are the other links in the original Atomspace contain this variable
        HandleSeq incomings = originalAtomSpace->getIncoming( ((Handle)(varIt->second)));
        foreach(Handle incomingHandle, incomings)
        {
            if (isInHandleSeq(incomingHandle, instance))
                continue;

            Handle extendedHandle;
            // if this atom is a igonred type, get its first parent that is not in the igonred types
            if (isIgnoredType (originalAtomSpace->getType(incomingHandle)) )
            {
                extendedHandle = getFirstNonIgnoredIncomingLink(originalAtomSpace, incomingHandle);
                if (extendedHandle == Handle::UNDEFINED)
                    continue;
            }
            else
                extendedHandle = incomingHandle;

            // Add this extendedHandle to the old pattern so as to make a new pattern
            HandleSeq originalLinks = instance;
            originalLinks.push_back(extendedHandle);

            // Extract all the possible patterns from this originalLinks, not duplicating the already existing patterns
            vector<HTreeNode*> newPatternNodes = extractAllPossiblePatternsFromInputLinks(originalLinks, gram);

            foreach (HTreeNode* HNode, newPatternNodes)
            {
                HNode->parentLinks.push_back(curHTreeNode);

                // Find All Instances in the original AtomSpace For this Pattern
                findAllInstancesForGivenPattern(HNode);
            }

        }
    }
}

void PatternMiner::growPatternsTask()
{
    static unsigned int cur_index = -1;

    vector<HTreeNode*>& last_gram_patterns = patternsForGram[cur_gram-1];

    unsigned int total = last_gram_patterns.size();

    while(true)
    {
        patternForLastGramLock.lock();
        cur_index ++;
        if (cur_index >= total)
            break;

        patternForLastGramLock.unlock();

        HTreeNode* cur_growing_pattern = last_gram_patterns[cur_index];
        foreach (HandleSeq instance , cur_growing_pattern->instances)
        {
            extendAllPossiblePatternsForOneMoreGram(instance, cur_growing_pattern, cur_gram);
        }

    }

}

bool compareHTreeNodeByFrequency(HTreeNode* node1, HTreeNode* node2)
{
    return (node1->instances.size() >= node2->instances.size());
}

void PatternMiner::OutPutPatternsToFile(unsigned int n_gram)
{
    // out put the n_gram patterns to a file
    ofstream resultFile;
    string fileName = "FrequentPatterns_" + toString(n_gram) + "gram.scm";
    resultFile.open(fileName.c_str());
    vector<HTreeNode*> &patternsForThisGram = patternsForGram[n_gram-1];
    resultFile << "Frequenc Pattern Mining results for " + toString(n_gram) + " gram patterns. Total pattern number: " + toString(patternsForThisGram.size()) << endl;

    foreach(HTreeNode* htreeNode, patternsForThisGram)
    {
        resultFile << endl << "Pattern: Frequency = " << toString(htreeNode->instances.size()) << endl;
        foreach (Handle link, htreeNode->pattern)
        {
            resultFile << atomSpace->atomAsString(link);
        }
    }

    resultFile.close();

}

void PatternMiner::ConstructTheFirstGramPatterns()
{
    cur_gram = 1;

    originalAtomSpace->getHandlesByType(back_inserter(allLinks), (Type) LINK, true );

    for (unsigned int i = 0; i < THREAD_NUM; ++ i)
    {
        threads[i] = std::thread([this]{this->growTheFirstGramPatternsTask();}); // using C++11 lambda-expression
        threads[i].join();
    }

    // sort the patterns by frequency
    std::sort((patternsForGram[0]).begin(), (patternsForGram[0]).end(),compareHTreeNodeByFrequency );
    OutPutPatternsToFile(1);

}

void PatternMiner::GrowAllPatterns()
{
    for ( cur_gram = 2; cur_gram <= MAX_GRAM; ++ cur_gram)
    {
        for (unsigned int i = 0; i < THREAD_NUM; ++ i)
        {
            threads[i] = std::thread([this]{this->growPatternsTask();}); // using C++11 lambda-expression
            threads[i].join();
        }

        std::sort((patternsForGram[cur_gram-1]).begin(), (patternsForGram[cur_gram-1]).end(),compareHTreeNodeByFrequency );

        // Finished mining cur_gram patterns; output to file
        OutPutPatternsToFile(cur_gram);
    }
}

PatternMiner::PatternMiner(AtomSpace* _originalAtomSpace, unsigned int max_gram): originalAtomSpace(_originalAtomSpace)
{
    htree = new HTree();
    atomSpace = new AtomSpace();

    unsigned int system_thread_num  = std::thread::hardware_concurrency();
    if (system_thread_num > 2)
        THREAD_NUM = system_thread_num - 2;
    else
        THREAD_NUM = 1;

    threads = new thread[THREAD_NUM];

    MAX_GRAM = max_gram;
    cur_gram = 0;

    ignoredTypes[0] = LIST_LINK;

    // vector < vector<HTreeNode*> > patternsForGram
    for (unsigned int i = 0; i < max_gram; ++i)
    {
        vector<HTreeNode*> patternVector;
        patternsForGram.push_back(patternVector);
    }
}

PatternMiner::~PatternMiner()
{
    delete htree;
    delete atomSpace;
}

void PatternMiner::runPatternMiner()
{

    // first, generate the first layer patterns: patterns of 1 gram (contains only one link)
    ConstructTheFirstGramPatterns();

    // and then generate all patterns
    GrowAllPatterns();

}
