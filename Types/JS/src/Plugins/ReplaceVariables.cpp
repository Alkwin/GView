#include "js.hpp"

namespace GView::Type::JS::Plugins
{
using namespace GView::View::LexicalViewer;

std::string_view ReplaceVariables::GetName()
{
    return "Replace strings";
}
std::string_view ReplaceVariables::GetDescription()
{
    return "Replace the variables with their string value.";
}
bool ReplaceVariables::CanBeAppliedOn(const GView::View::LexicalViewer::PluginData& data)
{
    for (auto index = data.startIndex; index < data.endIndex; index++)
    {
        if (data.tokens[index].GetTypeID(TokenType::None) == TokenType::String)
        {
            return true;
        }
    }
    return false;
}
GView::View::LexicalViewer::PluginAfterActionRequest ReplaceVariables::Execute(GView::View::LexicalViewer::PluginData& data)
{
    int32 index = (int32) data.endIndex - 1;
    LocalUnicodeStringBuilder<256> temp;
    while (index >= (int32) data.startIndex)
    {
        Token endToken = data.tokens[index];
        // Test code to see if the 'sentence is right'
        // if (endToken.GetTypeID(TokenType::None) == TokenType::String &&                          // "x"
        //     endToken.Precedent().GetTypeID(TokenType::None) == TokenType::Operator_Assignment && // =
        //     endToken.Precedent().Precedent().GetTypeID(TokenType::None) == TokenType::Word &&    // x
        //     endToken.Precedent().Precedent().Precedent().GetTypeID(TokenType::None) == TokenType::DataType_Var)
        // {
        //     Token start      = endToken.Precedent().Precedent().Precedent();
        //     index            = start.GetIndex();
        //     auto startOffset = start.GetTokenStartOffset();
        //     auto endOffset   = endToken.GetTokenEndOffset();
        //     if (!startOffset.has_value() || !endOffset.has_value())
        //         return GView::View::LexicalViewer::PluginAfterActionRequest::None;
        //     auto size = endOffset.value() - startOffset.value();

        //     data.editor.Replace(startOffset.value(), size, "WOAH");
        // }

        // '= x' OR '+ x' -> assignment or concatenation
        if (endToken.GetTypeID(TokenType::None) == TokenType::Word &&
            (endToken.Precedent().GetTypeID(TokenType::None) == TokenType::Operator_Assignment ||
             endToken.Precedent().GetTypeID(TokenType::None) == TokenType::Operator_Plus))
        {
            Token searchValue = endToken;
            // declaring the variable
            while (
                  !(searchValue.GetTypeID(TokenType::None) == TokenType::String &&                          // "x"
                    searchValue.Precedent().GetTypeID(TokenType::None) == TokenType::Operator_Assignment && // =
                    searchValue.Precedent().Precedent().GetTypeID(TokenType::None) == TokenType::Word &&    // x
                    searchValue.Precedent().Precedent().GetText() == endToken.GetText() &&                  // same name variable
                    searchValue.Precedent().Precedent().Precedent().GetTypeID(TokenType::None) == TokenType::DataType_Var)) // var
            {                                                                                                               // var x = "x"
                searchValue = searchValue.Precedent();
                if (searchValue.GetIndex() <= 0)
                    continue; // error situation, variable used that has not been declared?
            }
            temp.Clear();
            for (auto ch : searchValue.GetText())
            {
                temp.AddChar(ch);
            }

            index            = endToken.GetIndex();
            auto startOffset = endToken.GetTokenStartOffset();
            auto endOffset   = endToken.GetTokenEndOffset();
            if (!startOffset.has_value() || !endOffset.has_value())
                return GView::View::LexicalViewer::PluginAfterActionRequest::None;
            auto size = endOffset.value() - startOffset.value();

            data.editor.Replace(startOffset.value(), size, temp.ToStringView());
        }
        index--;
    }

    return GView::View::LexicalViewer::PluginAfterActionRequest::Rescan;
}
} // namespace GView::Type::JS::Plugins