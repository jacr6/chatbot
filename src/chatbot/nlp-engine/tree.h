/*
 * Copyright (C) 2012 Andres Pagliano, Gabriel Miretti, Gonzalo Buteler,
 * Nestor Bustamante, Pablo Perez de Angelis
 *
 * This file is part of LVK Chatbot.
 *
 * LVK Chatbot is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * LVK Chatbot is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with LVK Chatbot.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#ifndef LVK_NLP_TREE_H
#define LVK_NLP_TREE_H

#include <QString>
#include <QStringList>
#include <QPair>
#include <QList>
#include <QSet>

#include "nlp-engine/engine.h"
#include "nlp-engine/word.h"
#include "nlp-engine/result.h"
#include "nlp-engine/varstack.h"
#include "nlp-engine/parser.h"

namespace Lvk
{

/// \addtogroup Lvk
/// @{

namespace Nlp
{

class Rule;
class Node;
class MatchPolicy;
class ScoringAlgorithm;

/// \ingroup Lvk
/// \addtogroup Nlp
/// @{

/**
 * \brief The Tree class provides ...
 */
class Tree
{
public:

    /**
     * Constructs an empty tree
     */
    Tree();

    /**
     * Destroys the object
     */
    ~Tree();

    /**
     * Adds \a rule to the tree
     */
    void add(const Rule &rule);

    /**
     * Gets the list of responses for the given input
     */
    QStringList getResponses(const QString &input, Engine::MatchList &matches);

    /**
     * Gets the response with the highest score
     */
    QString getResponse(const QString &input, Engine::MatchList &matches);

private:
    Tree(Tree&);
    Tree& operator=(Tree&);

    typedef QPair<int, Nlp::Node *> PairedNode; // pair (input idx, node)

    Node *m_root;
    MatchPolicy *m_matchPolicy;
    ScoringAlgorithm *m_scoringAlg;
    Nlp::Parser m_parser;
    Nlp::VarStack m_stack;
    QSet< QPair<const Nlp::Node*, int> > m_loopDetector;

    Nlp::Node * addNode(const Nlp::Word &word, Nlp::Node *parent);
    void addNodeOutput(const Rule &rule, const QSet<PairedNode> &onodes);
    void scoredDFS(ResultList &r, const Nlp::Node *root, const Nlp::WordList &words,
                   int offset = 0);
    void handleEndWord(Nlp::ResultList &results, const Nlp::Node *node, int offset);
    Nlp::Result getValidOutput(const Nlp::Node *node);
    QString expandVars(const QString &output, bool *ok);
    void parseRuleInput(const QString &input, Nlp::WordList &words);
    void parseUserInput(const QString &input, Nlp::WordList &words);
    void filterSymbols(Nlp::WordList &words);
    void parseExactMatch(Nlp::WordList &words);
    QString getRecResponse(const QString &input, Engine::MatchList &matches);
    QString getVarValue(const QString &varName);
    void updateVarStack(const Node *node, int offset, const Nlp::Word &word, float matchWeight);
};

/// @}

} // namespace Nlp

/// @}

} // namespace Lvk


#endif // LVK_NLP_TREE_H
