#include "designer/core/json_document.hpp"

#include <cstdio>
#include <fstream>
#include <sstream>

namespace designer
{

using designer::Json;

namespace
{

std::string unescapeToken(std::string token)
{
    // JSON Pointer 的 ~1 -> '/'、~0 -> '~'
    std::string out;
    out.reserve(token.size());
    for (size_t i = 0; i < token.size(); i++)
    {
        if (token[i] == '~' && i + 1 < token.size())
        {
            if (token[i + 1] == '1')
            {
                out.push_back('/');
                i++;
                continue;
            }
            if (token[i + 1] == '0')
            {
                out.push_back('~');
                i++;
                continue;
            }
        }
        out.push_back(token[i]);
    }
    return out;
}

std::string escapeToken(const std::string& token)
{
    std::string out;
    out.reserve(token.size());
    for (char c : token)
    {
        if (c == '~')
            out += "~0";
        else if (c == '/')
            out += "~1";
        else
            out.push_back(c);
    }
    return out;
}

bool isIndex(const std::string& token, size_t& index)
{
    if (token.empty())
        return false;
    for (char c : token)
        if (c < '0' || c > '9')
            return false;
    index = (size_t)std::strtoul(token.c_str(), nullptr, 10);
    return true;
}

Json* walk(Json& node, const std::vector<std::string>& tokens, size_t upto)
{
    Json* current = &node;
    for (size_t i = 0; i < upto; i++)
    {
        const std::string& token = tokens[i];
        if (current->is_object())
        {
            auto it = current->find(token);
            if (it == current->end())
                return nullptr;
            current = &(*it);
        }
        else if (current->is_array())
        {
            size_t index = 0;
            if (!isIndex(token, index) || index >= current->size())
                return nullptr;
            current = &(*current)[index];
        }
        else
        {
            return nullptr;
        }
    }
    return current;
}

const Json* walk(const Json& node, const std::vector<std::string>& tokens, size_t upto)
{
    return walk(const_cast<Json&>(node), tokens, upto);
}

} // namespace

Pointer Pointer::parse(const std::string& text)
{
    Pointer pointer;
    if (text.empty() || text[0] != '/')
        return pointer;

    size_t start = 1;
    while (start <= text.size())
    {
        const size_t slash = text.find('/', start);
        const size_t end   = (slash == std::string::npos) ? text.size() : slash;
        pointer.tokens.push_back(unescapeToken(text.substr(start, end - start)));
        if (slash == std::string::npos)
            break;
        start = slash + 1;
    }
    return pointer;
}

std::string Pointer::toString() const
{
    std::string out;
    for (const std::string& token : this->tokens)
        out += "/" + escapeToken(token);
    return out;
}

Pointer Pointer::parent() const
{
    Pointer out;
    if (this->tokens.size() > 1)
        out.tokens.assign(this->tokens.begin(), this->tokens.end() - 1);
    return out;
}

bool JsonDocument::loadFile(const std::string& path, std::string& error)
{
    std::ifstream in(path, std::ios::binary);
    if (!in)
    {
        error = "打不开文件：" + path;
        this->lastError = error;
        return false;
    }
    std::ostringstream buffer;
    buffer << in.rdbuf();

    if (!this->fromText(buffer.str(), error))
        return false;

    this->filePath = path;
    return true;
}

bool JsonDocument::fromText(const std::string& text, std::string& error)
{
    try
    {
        this->document = Json::parse(text, nullptr, true, true); // 允许注释，和库的解析口径一致
    }
    catch (const std::exception& e)
    {
        error           = std::string("JSON 语法错误：") + e.what();
        this->lastError = error;
        return false;
    }

    if (!this->document.is_object() && !this->document.is_array())
    {
        error           = "顶层必须是对象或数组";
        this->lastError = error;
        return false;
    }

    this->savedText = this->serialize();
    this->hasSaved  = true;
    return true;
}

const Json* JsonDocument::get(const Pointer& pointer) const
{
    const Json* parent = walk(this->document, pointer.tokens, pointer.tokens.size());
    return parent;
}

const Json* JsonDocument::get(const std::string& pointerText) const
{
    return this->get(Pointer::parse(pointerText));
}

bool JsonDocument::set(const Pointer& pointer, Json value, std::string& error)
{
    if (pointer.empty())
    {
        if (!value.is_object() && !value.is_array())
        {
            error = "顶层只能写成对象或数组";
            return false;
        }
        this->document = std::move(value);
        return true;
    }

    Json* parent = walk(this->document, pointer.tokens, pointer.tokens.size() - 1);
    if (parent == nullptr)
    {
        error = "路径的父级不存在：" + pointer.parent().toString();
        return false;
    }

    const std::string& last = pointer.back();
    if (parent->is_object())
    {
        (*parent)[last] = std::move(value);
        return true;
    }
    if (parent->is_array())
    {
        size_t index = 0;
        if (!isIndex(last, index))
        {
            error = "数组下标不是数字：" + last;
            return false;
        }
        if (index < parent->size())
        {
            (*parent)[index] = std::move(value);
            return true;
        }
        if (index == parent->size())
        {
            parent->push_back(std::move(value));
            return true;
        }
        error = "数组下标越界：" + last + "（长度 " + std::to_string(parent->size()) + "）";
        return false;
    }

    error = "父级既不是对象也不是数组：" + pointer.parent().toString();
    return false;
}

bool JsonDocument::set(const std::string& pointerText, Json value, std::string& error)
{
    return this->set(Pointer::parse(pointerText), std::move(value), error);
}

bool JsonDocument::remove(const Pointer& pointer, std::string& error)
{
    if (pointer.empty())
    {
        error = "不能删除根节点";
        return false;
    }

    Json* parent = walk(this->document, pointer.tokens, pointer.tokens.size() - 1);
    if (parent == nullptr)
    {
        error = "路径的父级不存在：" + pointer.parent().toString();
        return false;
    }

    const std::string& last = pointer.back();
    if (parent->is_object())
    {
        if (parent->erase(last) == 0)
        {
            error = "键不存在：" + last;
            return false;
        }
        return true;
    }
    if (parent->is_array())
    {
        size_t index = 0;
        if (!isIndex(last, index) || index >= parent->size())
        {
            error = "数组下标越界：" + last;
            return false;
        }
        parent->erase(parent->begin() + (std::ptrdiff_t)index);
        return true;
    }

    error = "父级既不是对象也不是数组";
    return false;
}

bool JsonDocument::remove(const std::string& pointerText, std::string& error)
{
    return this->remove(Pointer::parse(pointerText), error);
}

std::string JsonDocument::serialize() const
{
    return this->document.dump(2) + "\n";
}

bool JsonDocument::isDirty() const
{
    return !this->hasSaved || this->serialize() != this->savedText;
}

} // namespace designer
