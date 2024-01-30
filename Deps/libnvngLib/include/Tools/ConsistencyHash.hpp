#pragma once

#include <map>
#include <vector>
#include <thread>
#include <random>
#include <assert.h>

template <NonCopyableConcept T>
class Node;

template <NonCopyableConcept T>
class INode
{
public:
        typedef Node<T> NodeType;

        virtual ~INode() {}
        virtual NodeType* GetNode() = 0;
};

template <NonCopyableConcept T>
class Node : public INode<T>
{
public:
        template<typename... Args>
        Node(const Args&... args) : _data(std::forward<const Args&>(args)...)
        {
        }

        Node* GetNode() override { return this; }

        const T _data;
};

template <NonCopyableConcept T>
class VNode : public INode<T>
{
public:
        typedef Node<T> NodeType;
        VNode(NodeType* node) : _node(node)
        {
        }

        NodeType* GetNode() override { return _node; }

private:
        NodeType* _node = nullptr;
};

template <NonCopyableConcept T>
class ConsistencyHash final
{
public:
        typedef INode<T> INodeType;
        typedef Node<T>  NodeType;
        typedef VNode<T> VNodeType;

        explicit ConsistencyHash(int32_t ratio = 100)
                : _ratio(ratio)
        {
                std::lock_guard l(_nodeListMutex);
                _ratio = std::max(10, std::min(ratio, 10000));
        }

        virtual ~ConsistencyHash()
        {
                std::lock_guard l(_nodeListMutex);
                for (auto& val : _nodeList)
                        delete val.second;
                _nodeList.clear();
        }

        template <typename... Args>
        inline std::size_t AddNode(int32_t weights, const Args& ... args)
        {
                std::lock_guard l(_nodeListMutex);
                weights = std::max(1, std::min(weights, 10));
                weights *= _ratio;

                NodeType* node = new NodeType(std::forward<const Args&>(args)...);

                const std::string origKey = node->_data.GetKey();
                std::mt19937_64 gen;
                std::seed_seq sed(origKey.begin(), origKey.end());
                gen.seed(sed);

                auto genHashKey = [&]()
                {
                        char buff[1024 * 4] = { 0 };
                        snprintf(buff, sizeof(buff), "%ld:%s:%ld", gen(), origKey.c_str(), gen());
                        return Hash<std::string>(buff);
                };

                std::size_t retKey = genHashKey();
                bool ret = _nodeList.emplace(retKey, node).second;
                if (!ret)
                {
                        delete node;
                        return 0;
                }

                for (int32_t i = 0; i < weights; ++i)
                {
                        VNodeType* vnode = new VNodeType(node);
                        bool ret_ = _nodeList.emplace(genHashKey(), vnode).second;
                        if (!ret_)
                        {
                                delete vnode;
                        }
                }

                return retKey;
        }

        template <typename... Args>
        inline std::size_t AddNodeNormal(int32_t weights, const Args& ... args)
        {
                std::lock_guard l(_nodeListMutex);
                weights = std::max(1, std::min(weights, 10));
                weights *= _ratio;

                NodeType* node = new NodeType(std::forward<const Args&>(args)...);

                const std::string origKey = node->_data.GetKey();
                auto genHashKey = [&](int64_t idx)
                {
                        char buff[1024 * 4] = { 0 };
                        snprintf(buff, sizeof(buff), "%s&&VN%ld", origKey.c_str(), idx);
                        return Hash<std::string>(buff);
                };

                std::size_t retKey = genHashKey(0);
                bool ret = _nodeList.emplace(retKey, node).second;
                if (!ret)
                {
                        delete node;
                        return 0;
                }

                for (int32_t i = 1; i < weights; ++i)
                {
                        VNodeType* vnode = new VNodeType(node);
                        bool ret_ = _nodeList.emplace(genHashKey(i), vnode).second;
                        if (!ret_)
                        {
                                delete vnode;
                        }
                }

                return retKey;
        }

        std::vector<std::size_t> GetDiffList()
        {
                std::lock_guard l(_nodeListMutex);
                std::vector<std::size_t> ret;
                if (_nodeList.empty())
                        return ret;

                std::size_t firstDiff = (std::numeric_limits<std::size_t>::max() - (_nodeList.rbegin()->first + 1)) + _nodeList.begin()->first;
                ret.push_back(firstDiff);

                auto it = _nodeList.begin();
                auto ie = _nodeList.end();
                auto beforeIt = it;
                ++it;
                for (; ie != it; ++it)
                {
                        std::size_t diff = (it->first - beforeIt->first + 1);
                        ret.push_back(diff);
                        beforeIt = it;
                }

                return ret;
        }

        inline void RemoveNode(std::size_t hashKey)
        {
                std::lock_guard l(_nodeListMutex);

                auto it = _nodeList.find(hashKey);
                if (_nodeList.end() != it)
                {
                        INodeType* node = it->second;
                        _nodeList.erase(it);

                        auto it = _nodeList.begin();
                        while (_nodeList.end() != it)
                        {
                                if (it->second->GetNode() == node)
                                {
                                        delete it->second;
                                        it = _nodeList.erase(it);
                                }
                                else
                                {
                                        ++it;
                                }
                        }

                        delete node;
                }
        }

        template <typename U>
        inline const T& GetNodeByKey(const U& key)
        {
                return GetNodeByKeyInternal(Hash<U>(key));
        }

private:
        template <typename U>
        inline std::size_t Hash(const U& key)
        {
                static std::hash<U> hasher;
                return hasher(key);
        }

        inline const T& GetNodeByKeyInternal(std::size_t hashKey)
        {
                std::lock_guard l(_nodeListMutex);
                static const T emptyObj;
                if (_nodeList.empty())
                {
                        // assert(false);
                        return emptyObj;
                }

                INodeType* retNode = nullptr;
                auto it = _nodeList.upper_bound(hashKey);
                if (_nodeList.end() != it)
                {
                        retNode = it->second;
                }
                else
                {
                        retNode = _nodeList.begin()->second;
                }

                NodeType* ret = retNode->GetNode();
                if (nullptr == ret)
                {
                        // assert(false);
                        return emptyObj;
                }

                return ret->_data;
        }

private:
        int32_t _ratio;
        SpinLock _nodeListMutex;
        std::map<std::size_t, INodeType*> _nodeList;
};

// vim: fenc=utf8:expandtab:ts=8:noma
