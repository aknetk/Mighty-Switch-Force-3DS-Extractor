#ifndef HASHMAP_H
#define HASHMAP_H

#include <stdio.h>
#include <functional>

template <typename T> class HashMapElement {
public:
    uint32_t Key = 0x00000000U;
    bool   Used = false;
    T      Data;
};
template <typename T> class HashMap {
public:
    int Count = 0;
    int Capacity = 0;
    int ChainLength = 16;
    HashMapElement<T>* Data = NULL;

    uint32_t HashFunction(const char*) {
        uint32_t hash = 0x811C9DC5U;

        while (*message) {
            hash ^= *message;
            hash *= 0x1000193U;
            message++;
        }
        return hash;
    }

    HashMap<T>(uint32_t (*hashFunc)(const char*) = NULL, int capacity = 16) {
        Count = 0;
        Capacity = capacity;

        Data = (HashMapElement<T>*)calloc(Capacity, sizeof(HashMapElement<T>));
    	if (!Data) {
            printf("Could not allocate memory for HashMap data!\n");
            exit(0);
        }
    }

    void Dispose() {
        if (Data)
            free(Data);
        Data = NULL;
    }
    ~HashMap<T>() {
        Dispose();
    }

    uint32_t TranslateIndex(uint32_t index) {
        // Find index that works to put new data in.
        index += (index << 12);
        index ^= (index >> 22);
        index += (index << 4);
        index ^= (index >> 9);
        index += (index << 10);
        index ^= (index >> 2);
        index += (index << 7);
        index ^= (index >> 12);
        index = (index >> 3) * 0x9E3779B1U;
        index = index & (Capacity - 1); // index = index % Capacity;
        return index;
    }
    int      Resize() {
        Capacity <<= 1;
        HashMapElement<T>* oldData = Data;

        Data = (HashMapElement<T>*)calloc(Capacity, sizeof(HashMapElement<T>));
        if (!Data) {
            printf("Could not allocate memory for HashMap data!\n");
            exit(0);
        }

        Count = 0;
        uint32_t index;
        for (int i = 0; i < Capacity / 2; i++) {
            if (oldData[i].Used) {
                index = TranslateIndex(oldData[i].Key);

                for (int c = 0; c < ChainLength; c++) {
                    if (!Data[index].Used) {
                        Count++;
            			break;
                    }
            		if (Data[index].Used && Data[index].Key == oldData[i].Key)
            			break;
            		index = (index + 1) & (Capacity - 1); // index = (index + 1) % Capacity;
            	}

                Data[index].Key = oldData[i].Key;
                Data[index].Used = true;
                Data[index].Data = oldData[i].Data;
            }
        }

        free(oldData);

        return Capacity;
    }

    void     Put(uint32_t hash, T data) {
        uint32_t index;
        do {
            index = hash;
            index = TranslateIndex(index);

            for (int i = 0; i < ChainLength; i++) {
                if (!Data[index].Used) {
                    Count++;
                    if (Count >= Capacity / 2) {
                        index = 0xFFFFFFFFU;
                        Resize();
                        break;
                    }
        			break;
                }

        		if (Data[index].Used && Data[index].Key == hash)
        			break;

        		index = (index + 1) & (Capacity - 1); // index = (index + 1) % Capacity;
        	}
        }
        while (index == 0xFFFFFFFFU);

        Data[index].Key = hash;
        Data[index].Used = true;
        Data[index].Data = data;
    }
    void     Put(const char* key, T data) {
        uint32_t hash = HashFunction(key);
        Put(hash, data);
    }
    T        Get(uint32_t hash) {
        uint32_t index;

        index = hash;
        index = TranslateIndex(index);

        for (int i = 0; i < ChainLength; i++) {
            if (Data[index].Used && Data[index].Key == hash) {
                return Data[index].Data;
            }

            index = (index + 1) & (Capacity - 1); // index = (index + 1) % Capacity;
        }

        return T { 0 };
    }
    T        Get(const char* key) {
        uint32_t hash = HashFunction(key);
        return Get(hash);
    }
    bool     Exists(uint32_t hash) {
        uint32_t index;

        index = hash;
        index = TranslateIndex(index);

        for (int i = 0; i < ChainLength; i++) {
            // printf("Data[%d].Key (%X) == %X: %s     .Used == %s\n",
            //     index, Data[index].Key, hash,
            //     Data[index].Key == hash ? "TRUE" : "FALSE",
            //     Data[index].Used ? "TRUE" : "FALSE");
            if (Data[index].Used && Data[index].Key == hash) {
                return true;
            }

            index = (index + 1) & (Capacity - 1); // index = (index + 1) % Capacity;
        }

        return false;
    }
    bool     Exists(const char* key) {
        uint32_t hash = HashFunction(key);
        return Exists(hash);
    }

    bool     Remove(uint32_t hash) {
        uint32_t index;

        index = hash;
        index = TranslateIndex(index);

        for (int i = 0; i < ChainLength; i++) {
            if (Data[index].Used && Data[index].Key == hash) {
                Count--;
                Data[index].Used = false;
                return true;
            }

            index = (index + 1) & (Capacity - 1); // index = (index + 1) % Capacity;
        }
        return false;
    }
    bool     Remove(const char* key) {
        uint32_t hash = HashFunction(key);
        return Remove(hash);
    }

    void     Clear() {
        for (int i = 0; i < Capacity; i++) {
            if (Data[i].Used) {
                Data[i].Used = false;
            }
        }
    }

    void     ForAll(void (*forFunc)(uint32_t, T)) {
        for (int i = 0; i < Capacity; i++) {
            if (Data[i].Used) {
                forFunc(Data[i].Key, Data[i].Data);
            }
        }
    }
    void     WithAll(std::function<void(uint32_t, T)> forFunc) {
        for (int i = 0; i < Capacity; i++) {
            if (Data[i].Used) {
                forFunc(Data[i].Key, Data[i].Data);
            }
        }
    }

    void     PrintHashes() {
        printf("Printing...\n");
        for (int i = 0; i < Capacity; i++) {
            if (Data[i].Used) {
                printf("Data[%d].Key: %X\n", i, Data[i].Key);
            }
        }
    }

    Uint8*   GetBytes(bool exportHashes) {
        uint32_t stride = ((exportHashes ? 4 : 0) + sizeof(T));
        Uint8* bytes = (Uint8*)malloc(Count * stride);
        if (exportHashes) {
            for (int i = 0, index = 0; i < Capacity; i++) {
                if (Data[i].Used) {
                    *(uint32_t*)(bytes + index * stride) = Data[i].Key;
                    *(T*)(bytes + index * stride + 4) = Data[i].Data;
                    index++;
                }
            }
        }
        else {
            for (int i = 0, index = 0; i < Capacity; i++) {
                if (Data[i].Used) {
                    *(T*)(bytes + index * stride) = Data[i].Data;
                    index++;
                }
            }
        }
        return bytes;
    }
    void     FromBytes(Uint8* bytes, int count) {
        uint32_t stride = (4 + sizeof(T));
        for (int i = 0; i < count; i++) {
            Put(*(uint32_t*)(bytes + i * stride),
                *(T*)(bytes + i * stride + 4));
        }
    }
};

#endif
