#include "js.hpp"

namespace GView::Type::JS::Plugins
{
using namespace GView::View::LexicalViewer;

std::string_view RemoveComments::GetName()
{
    return "Remove comments";
}
std::string_view RemoveComments::GetDescription()
{
    return "Remove comments.";
}
bool RemoveComments::CanBeAppliedOn(const GView::View::LexicalViewer::PluginData& data)
{
    for (auto index = data.startIndex; index < data.endIndex; index++)
    {
        if (data.tokens[index].GetTypeID(TokenType::None) == TokenType::Comment)
        {
            return true;
        }
    }
    return false;
}
GView::View::LexicalViewer::PluginAfterActionRequest RemoveComments::Execute(GView::View::LexicalViewer::PluginData& data)
{
    int32 index = (int32) data.endIndex - 1;
    LocalUnicodeStringBuilder<256> temp;
    while (index >= (int32) data.startIndex)
    {
        Token endToken = data.tokens[index];

        if (endToken.GetTypeID(TokenType::None) == TokenType::Comment)
        {
            index            = endToken.GetIndex();
            auto startOffset = endToken.GetTokenStartOffset();
            auto endOffset   = endToken.GetTokenEndOffset();
            if (!startOffset.has_value() || !endOffset.has_value())
                return GView::View::LexicalViewer::PluginAfterActionRequest::None;
            auto size = endOffset.value() - startOffset.value();

            data.editor.Replace(startOffset.value(), size, "");
        }
        index--;
    }

    return GView::View::LexicalViewer::PluginAfterActionRequest::Rescan;
}
} // namespace GView::Type::JS::Plugins