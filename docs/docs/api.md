# API

## Prerequisites
What is JSON pointer? https://jsonpatch.com/#json-pointer

## class Automerge
### methods 
```
void json_add(const json::json_pointer& path, const json& value)
    @brief	add a new item with a new path, the parent path should exist
    @param[in] path     A json path
    @param[in] value    Json value to be added at the path
    @throw 

    @note	commit() should be called manually after this operation
```

```
void json_replace(const json::json_pointer& path, const json& value)
    @brief	replace an item at an existed path
    @param[in] path      A json path
    @param[in] value    Json value to be replaced at the path
    @throw

    @note	commit() should be called manually after this operation.
            The original item will be replaced by the new item entirely. No diff operating internal.
            Change several items in an object, should call replace() separately for each item.
```

```
void json_delete(const json::json_pointer& path)
    @brief 	delete a item(scalar or object) of an existed path
    @param[in] path         A json path
    @throw

    @note	commit() should be called manually after this operation
```

```
void commit()
    @brief	commit a batch of operations together in a transaction

    @note	A user operation usually contains several Automerge operations. commit() should be called
            after all add/replace/delete operations applied of one user operation.
```
