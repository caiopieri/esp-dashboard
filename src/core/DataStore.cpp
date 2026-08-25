#include "DataStore.h"
#include <ctype.h>

bool DataStore::validToken(const String& token) {
    if (token.length() == 0 || token.length() > 32) return false;
    for (size_t i = 0; i < token.length(); ++i) {
        const unsigned char character = static_cast<unsigned char>(token[i]);
        if (!(isalnum(character) || character == '_' || character == '-' || character == '.')) return false;
    }
    return true;
}

int DataStore::find(const String& namespaceName, const String& key) const {
    for (size_t i = 0; i < _count; ++i) {
        if (_entries[i].namespaceName == namespaceName && _entries[i].key == key) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

bool DataStore::set(const String& namespaceName, const String& key, const String& value) {
    if (!validToken(namespaceName) || !validToken(key) || value.length() > 256) return false;

    int index = find(namespaceName, key);
    if (index < 0) {
        if (_count >= MAX_ENTRIES) return false;
        index = static_cast<int>(_count++);
        _entries[index].namespaceName = namespaceName;
        _entries[index].key = key;
    }
    _entries[index].value = value;
    return true;
}

bool DataStore::get(const String& namespaceName, const String& key, String& value) const {
    const int index = find(namespaceName, key);
    if (index < 0) return false;
    value = _entries[index].value;
    return true;
}

void DataStore::clear() {
    _count = 0;
}
