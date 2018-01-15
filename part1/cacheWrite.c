/* Summer 2017 */
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include "utils.h"
#include "cacheWrite.h"
#include "getFromCache.h"
#include "mem.h"
#include "setInCache.h"
#include "../part2/hitRate.h"

/*
	Takes in a cache and a block number and evicts the block at that number
	from the cache. This does not change any of the bits in the cache but
	checks if data needs to be written to main memory or and then makes
	calls to the appropriate functions to do so.
*/
void evict(cache_t* cache, uint32_t blockNumber) {
	uint8_t valid = getValid(cache, blockNumber);
	uint8_t dirty = getDirty(cache, blockNumber);
	//printf("Evicting block %u\n", blockNumber);
	if (valid && dirty) {
		uint32_t tag = extractTag(cache, blockNumber);
		uint32_t address = extractAddress(cache, tag, blockNumber, 0);
		writeToMem(cache, blockNumber, address);
	}
}

/*
	Takes in a cache, an address, a pointer to data, and a size of data
	and writes the updated data to the cache. If the data block is already
	in the cache it updates the contents and sets the dirty bit. If the
	contents are not in the cache it is written to a new slot and
	if necessary something is evicted from the cache.
*/
void writeToCache(cache_t* cache, uint32_t address, uint8_t* data, uint32_t dataSize) {
    /* Your Code Here. */
	if (data == NULL || cache == NULL) {
		return;
	}
	evictionInfo_t* blockInfo = findEviction(cache, address);
	reportAccess(cache);
	if (blockInfo->match == 1) {
		//printf("Writing to block %u\n", blockInfo->blockNumber);
		writeDataToCache(cache, address, data, dataSize, extractTag(cache, blockInfo->blockNumber), blockInfo);
		reportHit(cache);
		free(blockInfo);
		return;
	}
	evict(cache, blockInfo->blockNumber);												// If it is a miss, evict (write to mem if dirty)
	uint8_t* memData = readFromMem(cache, address - getOffset(cache, address));			// Get the whole block
	setData(cache, memData, blockInfo->blockNumber, cache->blockDataSize, 0);			// Write block to cache
	setTag(cache, getTag(cache, address), blockInfo->blockNumber);						// Set new tag
	writeDataToCache(cache, address, data, dataSize, extractTag(cache, blockInfo->blockNumber), blockInfo);	// Finally do the writing
	free(memData);
	free(blockInfo);
	return;
}

/*
	Takes in a cache, an address to write to, a pointer containing the data
	to write, the size of the data, a tag, and a pointer to an evictionInfo
	struct and writes the data given to the cache based upon the location
	given by the evictionInfo struct.
*/
void writeDataToCache(cache_t* cache, uint32_t address, uint8_t* data, uint32_t dataSize, uint32_t tag, evictionInfo_t* evictionInfo) {
	uint32_t idx = getIndex(cache, address);
	setData(cache, data, evictionInfo->blockNumber, dataSize , getOffset(cache, address));
	setDirty(cache, evictionInfo->blockNumber, 1);
	setValid(cache, evictionInfo->blockNumber, 1);
	setShared(cache, evictionInfo->blockNumber, 0);
	updateLRU(cache, tag, idx, evictionInfo->LRU);
}

/*
	Takes in a cache, an address, and a byte of data and writes the byte
	of data to the cache. May evict something if the block is not already
	in the cache which may also require a fetch from memory. Returns -1
	if the address is invalid, otherwise 0.
*/
int writeByte(cache_t* cache, uint32_t address, uint8_t data) {
	if (cache == NULL || validAddresses(address, (uint32_t) 1) != 1)
		return -1;
	uint8_t* dataArray = (uint8_t *) malloc(sizeof(uint8_t));
	if (dataArray == NULL) {
		return -1;
	}
	*dataArray = data;
	writeToCache(cache, address, dataArray, (uint32_t) 1);
	free(dataArray);
	return 0;
}

/*
	Takes in a cache, an address, and a halfword of data and writes the
	data to the cache. May evict something if the block is not already
	in the cache which may also require a fetch from memory. Returns 0
	for a success and -1 if there is an allignment error or an invalid
	address was used.
*/
int writeHalfWord(cache_t* cache, uint32_t address, uint16_t data) {
	if (cache == NULL || validAddresses(address, (uint32_t) 2) != 1 || (address % 2 != 0))
		return -1;
	uint8_t* dataArray = (uint8_t *) malloc(sizeof(uint8_t) * 2);
	if (dataArray == NULL) {
		return -1;
	}
	int shift = 8;
	for (int i = 0; i < 2; i++) {
		*(dataArray + i) = (data >> shift) & 255;
		shift -= 8;
	}

	if (cache->blockDataSize < 2) {
		int shift = 8;
		for(uint32_t i = 0; i < 2; i ++) {
			writeByte(cache, address + i, (uint8_t) (data >> shift));
			shift -= 8;
		}
	} else {
		writeToCache(cache, address, dataArray, (uint32_t) 2);
	}

	free(dataArray);
	return 0;
}

/*
	Takes in a cache, an address, and a word of data and writes the
	data to the cache. May evict something if the block is not already
	in the cache which may also require a fetch from memory. Returns 0
	for a success and -1 if there is an allignment error or an invalid
	address was used.
*/
int writeWord(cache_t* cache, uint32_t address, uint32_t data) {
	if (cache == NULL || validAddresses(address, (uint32_t) 4) != 1 || (address % 4 != 0))
		return -1;
	uint8_t* dataArray = (uint8_t *) malloc(sizeof(uint8_t) * 4);
	if (dataArray == NULL) {
		return -1;
	}
	int shift = 24;
	for (int i = 0; i < 4; i++) {
		*(dataArray + i) = (uint8_t)(data >> shift);
		shift -= 8;
	}
	if (cache->blockDataSize < 4) {
		int shift = 16;
		for(uint32_t i = 0; i < 2; i ++) {
			writeHalfWord(cache, address + (2 * i), (uint16_t) (data >> shift));
			shift -= 16;
		}
	} else {
		writeToCache(cache, address, dataArray, (uint32_t) 4);
	}
	free(dataArray);
	return 0;
}

/*
	Takes in a cache, an address, and a double word of data and writes the
	data to the cache. May evict something if the block is not already
	in the cache which may also require a fetch from memory. Returns 0
	for a success and -1 if there is an allignment error or an invalid address
	was used.
*/
int writeDoubleWord(cache_t* cache, uint32_t address, uint64_t data) {
	if (cache == NULL || (validAddresses(address, (uint32_t) 8) != 1) || address % 8 != 0)
		return -1;
	uint8_t* dataArray = (uint8_t *) malloc(sizeof(uint8_t) * 8);
	if (dataArray == NULL) {
		return -1;
	}
	int shift = 56;
	for (int i = 0; i < 8; i++) {
		*(dataArray + i) = (data >> shift) & 255;
		shift -= 8;
	}
	if (cache->blockDataSize < 8) {
		int shift = 32;
		for(uint32_t i = 0; i < 2; i ++) {
			writeHalfWord(cache, address + (4 * i), (uint16_t) (data >> shift));
			shift -= 32;
		}
	} else {
		writeToCache(cache, address, dataArray, (uint32_t) 8);
	}
	free(dataArray);
	return 0;
}

/*
	A function used to write a whole block to a cache without pulling it from
	physical memory. This is useful to transfer information between caches
	without needing to take an intermediate step of going through main memory,
	a primary advantage of MOESI. Takes in a cache to write to, an address
	which is being written to, the block number that the data will be written
	to and an entire block of data from another cache.
*/
void writeWholeBlock(cache_t* cache, uint32_t address, uint32_t evictionBlockNumber, uint8_t* data) {
	uint32_t idx = getIndex(cache, address);
	uint32_t tagVal = getTag(cache, address);
	int oldLRU = getLRU(cache, evictionBlockNumber);
	evict(cache, evictionBlockNumber);
	setValid(cache, evictionBlockNumber, 1);
	setDirty(cache, evictionBlockNumber, 0);
	setTag(cache, tagVal, evictionBlockNumber);
	setData(cache, data, evictionBlockNumber, cache->blockDataSize, 0);
	updateLRU(cache, tagVal, idx, oldLRU);
}
