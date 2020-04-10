/*****************************************************************************
 *
 *  PROJECT:     Multi Theft Auto v1.0
 *  LICENSE:     See LICENSE in the top level directory
 *  FILE:        xml/CXMLFileImpl.cpp
 *  PURPOSE:     XML file class
 *
 *  Multi Theft Auto is available from http://www.multitheftauto.com/
 *
 *****************************************************************************/
#include "StdInc.h"
#include <ios>
#include <iostream>
#include <fstream>

CXMLFileImpl::CXMLFileImpl(const std::string& strFilename, bool bUsingIDs, bool bReadOnly)
    : m_ulID(INVALID_XML_ID), m_bUsingIDs(bUsingIDs), m_bReadOnly(bReadOnly), m_strFilename(strFilename)
{
    if (bUsingIDs)
        m_ulID = CXMLArray::PopUniqueID(this);
}

CXMLFileImpl::CXMLFileImpl(const std::string& strFilename, CXMLNode* pNode, bool bReadOnly)
    : m_ulID(INVALID_XML_ID), m_bReadOnly(bReadOnly), m_strFilename(strFilename)
{
    bool bUsingIDs = pNode->GetID() != INVALID_XML_ID;
    if (bUsingIDs)
        m_ulID = CXMLArray::PopUniqueID(this);

    // Create document
    m_pDocument = std::make_unique<pugi::xml_document>();

    // Copy root node info
    auto& root = reinterpret_cast<CXMLNodeImpl*>(pNode)->GetNode();
    m_pDocument->append_copy(root);

    // Construct Wrapper tree
    BuildWrapperTree(bUsingIDs);
}

CXMLFileImpl::~CXMLFileImpl()
{
    if (m_ulID != INVALID_XML_ID)
        CXMLArray::PushUniqueID(this);
}

bool CXMLFileImpl::Parse(std::vector<char>* pOutFileContents)
{
    if (!m_strFilename.empty())
    {
        // Reset previous file
        Reset();

        // Open file
        std::ifstream file(m_strFilename, std::ios::in | std::ios::ate);

        // Disable whitespace skipping
        file.unsetf(std::ios::skipws);

        std::vector<char> vecFileContents{};
        std::string       strFileContents;

        if (file.is_open())
        {
            std::streampos fileSize = file.tellg();

            vecFileContents.reserve(fileSize);
            file.seekg(0, std::ios::beg);
            vecFileContents.insert(vecFileContents.begin(), std::istream_iterator<char>(file), std::istream_iterator<char>());
            file.close();

            // Also copy to buffer if requested
            if (pOutFileContents)
                pOutFileContents->insert(pOutFileContents->begin(), vecFileContents.begin(), vecFileContents.end());

            for (const auto& text : vecFileContents)
                strFileContents += text;
        }
        else
            return false;

        // Load the xml
        m_pDocument = std::make_unique<pugi::xml_document>();
        m_parserResult = m_pDocument->load_string(strFileContents.c_str());
        if (!m_parserResult)
            return false;

        BuildWrapperTree(m_ulID != INVALID_XML_ID);
        return true;
    }
    return false;
}

void CXMLFileImpl::BuildWrapperTree(bool bUsingIDs)
{
    m_pRoot = WrapperTreeWalker(m_pDocument.get(), bUsingIDs);
}

std::unique_ptr<CXMLNodeImpl> CXMLFileImpl::WrapperTreeWalker(pugi::xml_node* node, bool bUsingIDs)
{
    // Construct wrapper for this node
    auto wrapperNode = std::make_unique<CXMLNodeImpl>(*node, bUsingIDs, nullptr);

    // Construct Attributes
    for (auto& attribute : node->attributes())
    {
        wrapperNode->AddAttribute(std::make_unique<CXMLAttributeImpl>(attribute, bUsingIDs));
    }

    // Recursively call on our children
    for (auto& child : node->children())
    {
        // Only for actual child nodes
        if (child.type() == pugi::node_element)
            wrapperNode->AddChild(WrapperTreeWalker(&child, bUsingIDs));
    }

    return wrapperNode;
}

void CXMLFileImpl::Reset()
{
    m_pRoot.reset(nullptr);
}

bool CXMLFileImpl::Write()
{
    // We have a filename?
    if (!m_strFilename.empty())
        return m_pDocument->save_file(m_strFilename.c_str());

    return false;
}

CXMLNode* CXMLFileImpl::CreateRootNode(const std::string& strTagName)
{
    if (m_pRoot)
    {
        m_pRoot->GetNode().set_name(strTagName.c_str());
        return GetRootNode();
    }
    else
    {
        m_pDocument = std::make_unique<pugi::xml_document>();
        auto innerRoot = m_pDocument->append_child(strTagName.c_str());
        auto rootWrapper = std::make_unique<CXMLNodeImpl>(*m_pDocument.get(), m_ulID != INVALID_XML_ID);
        rootWrapper->AddChild(std::make_unique<CXMLNodeImpl>(innerRoot, m_ulID != INVALID_XML_ID));
        m_pRoot = std::move(rootWrapper);
        return GetRootNode();
    }
}

CXMLNode* CXMLFileImpl::GetRootNode()
{
    // The root node for pugixml is the first child of the document node
    if (m_pRoot)
        return m_pRoot->GetChildren().front().get();
    return nullptr;
}

CXMLErrorCodes::Code CXMLFileImpl::GetLastError(std::string& strOut)
{
    auto parserStatus = m_parserResult.status;
    if (parserStatus == pugi::status_ok)
        return CXMLErrorCodes::NoError;

    strOut = m_parserResult.description();
    return CXMLErrorCodes::OtherError;
}
