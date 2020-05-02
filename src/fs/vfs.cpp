#include <fs/vfs.hpp>
#include <mm/kheap.hpp>

namespace fs {

    ISuperblock *VFS::m_rootFs;
    DEntry *VFS::m_fsTree;

    DEntry *DEntry::createNode(const char *name) {
        DEntry *newDEntry = new DEntry;
        newDEntry->mutex.init();
        newDEntry->name = strdup(name);
        newDEntry->nameHash = strhash(name);
        newDEntry->chld = nullptr;
        newDEntry->usedCount = 0;
        newDEntry->node = nullptr;
        newDEntry->isFilesystemRoot = false;
        newDEntry->isFilesystemRoot = false;
        newDEntry->mnt = nullptr;
        return newDEntry;
    }

    DEntry *DEntry::createChildNode(const char *name) {
        DEntry *newDEntry = createNode(name);
        if (chld != nullptr) {
            newDEntry->next = chld;
            newDEntry->prev = chld->prev;
            chld->prev->next = newDEntry;
            chld->prev = newDEntry;
        } else {
            newDEntry->next = newDEntry;
            newDEntry->prev = newDEntry;
        }
        newDEntry->par = this;
        newDEntry->chld = nullptr;
        newDEntry->sb = sb;
        return newDEntry;
    }

    DEntry *DEntry::softLookup(const char *name) {
        uint64_t hash = strhash(name);
        DEntry *current = chld;
        do {
            if (current->nameHash == hash) {
                if (streq(current->name, name)) {
                    return this;
                }
            }
            current = current->next;
        } while (current != chld);
        return nullptr;
    }

    DEntry *DEntry::hardLookup(const char *name) {
        if (node == nullptr) {
            return nullptr;
        }
        uint64_t num = node->lookup(name);
        if (num == 0) {
            return nullptr;
        }
        DEntry *newChld = createChildNode(name);
        newChld->node = sb->getNode(num);
        return newChld;
    }

    void DEntry::incrementUsedCount() { atomicIncrement(&usedCount); }

    void DEntry::decrementUsedCount() { atomicDecrement(&usedCount); }

    void DEntry::dispose() {
        delete name;
        sb->dropNode(node->num);
        delete this;
    }

    void DEntry::cut() {
        prev->next = next;
        next->prev = prev;
        next = nullptr;
        prev = nullptr;
    }

    bool DEntry::drop() {
        if (mutex.someoneWaiting()) {
            return false;
        }
        par->mutex.lock();
        if (usedCount > 0) {
            par->mutex.unlock();
            return false;
        }
        if (mutex.someoneWaiting()) {
            par->mutex.unlock();
            return false;
        }
        par->decrementUsedCount();
        cut();
        par->mutex.unlock();
        return true;
    }

    DEntry *DEntry::goToParent() {
        if (par == nullptr) {
            return this;
        }
        mutex.lock();
        decrementUsedCount();
        DEntry *result = par;
        // TODO: atomic increment
        // invariant: parent and this node are alive
        // while dispose is not called
        par->incrementUsedCount();
        if (usedCount > 0) {
            mutex.unlock();
        } else {
            bool result = drop();
            mutex.unlock();
            if (result) {
                dispose();
            }
        }
        return result;
    }

    DEntry *DEntry::goToChild(const char *name) {
        mutex.lock();
        // search in cache
        DEntry *result = softLookup(name);
        if (result == nullptr) {
            // add it to cache
            result = hardLookup(name);
            if (result == nullptr) {
                decrementUsedCount();
                mutex.unlock();
                return nullptr;
            }
        }
        result->incrementUsedCount();
        usedCount--;
        mutex.unlock();
        return result;
    }

    void DEntry::dropRec() {
        DEntry *current = this;
        while (current != nullptr) {
            current->mutex.lock();
            if (current->usedCount > 0) {
                current->mutex.unlock();
                return;
            }
            if (!current->drop()) {
                return;
            }
            current->mutex.unlock();
            DEntry *next = current->par;
            dispose();
            current = next;
        }
    }

    DEntry *DEntry::walk(PathIterator *iter, bool resolveLast) {
        DEntry *current = this;
        // Observing this node. Current is not going to be deleted here
        // because
        // 1) it is fs root so it is always mounted and preserved
        // 2) it is opened folder => usedCount at least one
        // incrementing is only done to ensure goToChild/goToParent validity
        current->incrementUsedCount();
        while (!iter->atEnd(resolveLast)) {
            if (*(iter->get()) == '\0' || streq(iter->get(), ".")) {
                continue;
            }
            if (streq(iter->get(), "..")) {
                current = current->goToParent();
                continue;
            }
            current = current->goToChild(iter->get());
            if (current == nullptr) {
                decrementUsedCount();
                dropRec();
                return nullptr;
            }
        }
        return current;
    }

    void VFS::init(ISuperblock *sb) {
        m_rootFs = sb;
        m_rootFs->mount();
        m_fsTree = DEntry::createNode("/");
        m_fsTree->next = m_fsTree;
        m_fsTree->prev = m_fsTree;
        m_fsTree->par = m_fsTree;
        m_fsTree->isFilesystemRoot = true;
        m_fsTree->mutex.init();
        m_fsTree->sb = sb;
        m_fsTree->node = sb->getNode(sb->getRootNum());
        m_fsTree->incrementUsedCount();
    }

    IFile *VFS::open(const char *path, int perm) {
        PathIterator iter(path);
        DEntry *entry = m_fsTree->walk(&iter);
        entry->incrementUsedCount();
        return entry->node->open(perm);
    }

    void close(IFile *file) {
        file->finalize();
        file->entry->decrementUsedCount();
        file->entry->dropRec();
        delete file;
    }

}; // namespace fs