/* Summer 2017 */
#include "coherenceUtils.h"
#include "coherenceWrite.h"
#include "../part1/mem.h"
#include "../part1/getFromCache.h"
#include "../part1/setInCache.h"
#include "../part1/cacheWrite.h"
#include "../part1/cacheRead.h"
#include "../part2/hitRate.h"

/*
	A function which processes all cache writes for an entire cache system.
	Takes in a cache system, an address, an id for a cache, an address, a size
	of data, and a pointer to data and calls the appropriate functions on the
	cache being selected to write to the cache.
*/
void cacheSystemWrite(cacheSystem_t* cacheSystem, uint32_t address, uint8_t ID, uint8_t size, uint8_t* data) {
	uint8_t* transferData;
	evictionInfo_t* dstCacheInfo;
	evictionInfo_t* otherCacheInfo = NULL;
	uint32_t evictionBlockNumber;
	uint32_t offset;
	cacheNode_t** caches;
	//uint32_t tagVal;
	int otherCacheContains = 0;
	cache_t* dstCache = NULL;
	uint8_t counter = 0;
	caches = cacheSystem->caches;
	while (dstCache == NULL && counter < cacheSystem->size) { //Selects destination cache pointer from array of caches pointers
		if (caches[counter]->ID == ID) {
			dstCache = caches[counter]->cache;
		}
		counter++;
	}
	dstCacheInfo = findEviction(dstCache, address); //Finds block to evict and potential match
	evictionBlockNumber = dstCacheInfo->blockNumber;
	offset = getOffset(dstCache, address);
	if (dstCacheInfo->match) {
		/*What do you do if it is in the cache?*/
		writeToCache(dstCache, address, data, size); // WRITE TO IT
		if (returnFirstCacheID(cacheSystem->snooper, address, cacheSystem->blockDataSize) != -1) {
			otherCacheContains = 1;
		}
	} else {
		uint32_t oldAddress = extractAddress(dstCache, extractTag(dstCache, evictionBlockNumber), evictionBlockNumber, 0);
		/*How do you need to update the snooper?*/
		/*How do you need to update states for what is getting evicted (don't worry about evicting this will be handled at a later step when you place data in the cache)?*/
		removeFromSnooper(cacheSystem->snooper, oldAddress, ID, size);	// Not necessary to keep track anymore

		int otherID = returnIDIf1(cacheSystem->snooper, oldAddress, cacheSystem->blockDataSize);
		if (otherID != -1) { // If the other block is the only one that had the oldAddr
			setState(dstCache, evictionBlockNumber, INVALID); // ----
		}

		int val = returnFirstCacheID(cacheSystem->snooper, address, cacheSystem->blockDataSize);
		/*Check other caches???*/
		/*Your Code Here*/
		if (val != -1) { // ProbeWrite
			otherCacheContains = 1;
			otherCacheInfo = findEviction(getCacheFromID(cacheSystem, val), address);
			transferData = fetchBlock(getCacheFromID(cacheSystem, val), otherCacheInfo->blockNumber);	// Fetch the whole block
			writeWholeBlock(dstCache, address, evictionBlockNumber, transferData);
			free(transferData);
			free(otherCacheInfo);
		}

		if (!otherCacheContains) {
			/*Check Main memory?*/
			transferData = readFromMem(dstCache, address - offset);
			writeWholeBlock(dstCache, address, evictionBlockNumber, transferData);;
			writeToCache( dstCache, address, data, size);
			free(transferData);
		}

	}
	if (otherCacheContains) {
		/*What states need to be updated?*/
		/*Does anything else need to be editted?*/
		int removeID = returnFirstCacheID(cacheSystem->snooper, address, cacheSystem->blockDataSize);
		while (removeID != -1) {
			otherCacheInfo = findEviction(getCacheFromID(cacheSystem, removeID), address);
			setState(getCacheFromID(cacheSystem, removeID), otherCacheInfo->blockNumber, INVALID);
			removeFromSnooper(cacheSystem->snooper, address, removeID, cacheSystem->blockDataSize);
			removeID = returnFirstCacheID(cacheSystem->snooper, address, cacheSystem->blockDataSize);
			free(otherCacheInfo);
		}
	}
	setState(dstCache, evictionBlockNumber, MODIFIED);	// SET TO MODIFIED
	addToSnooper(cacheSystem->snooper, address, ID, cacheSystem->blockDataSize);

	free(dstCacheInfo);
}

/*
	A function used to write a byte to a specific cache in a cache system.
	Takes in a cache system, an address, an ID, and data for the cache which
	will be written to. Returns 0 if the write is successful and otherwise
	returns -1.
*/
int cacheSystemByteWrite(cacheSystem_t* cacheSystem, uint32_t address, uint8_t ID, uint8_t data) {
	/* Error Checking??*/
	if (cacheSystem == NULL || validAddresses(address, (uint32_t) 1) != 1 || (address % 1 != 0) || getCacheFromID(cacheSystem, ID) == NULL) {
		return -1;
	}
	uint8_t array[1];
	array[0] = data;
	cacheSystemWrite(cacheSystem, address, ID, 1, array);
	return 0;
}

/*
	A function used to write a halfword to a specific cache in a cache system.
	Takes in a cache system, an address, an ID, and data for the cache which
	will be written to. Returns 0 if the write is successful and otherwise
	returns -1.
*/
int cacheSystemHalfWordWrite(cacheSystem_t* cacheSystem, uint32_t address, uint8_t ID, uint16_t data) {
	/* Error Checking??*/
	if (cacheSystem == NULL || validAddresses(address, (uint32_t) 2) != 1 || (address % 2) != 0 || getCacheFromID(cacheSystem, ID) == NULL) {
		return -1;
	}
	if (cacheSystem->blockDataSize < 2) {
		cacheSystemByteWrite(cacheSystem, address, ID, (uint8_t) (data >> 8));
		cacheSystemByteWrite(cacheSystem, address + 1, ID, (uint8_t) (data & UINT8_MAX));
	}
	uint8_t array[2];
	array[0] = (uint8_t) (data >> 8);
	array[1] = (uint8_t) (data & UINT8_MAX);
	cacheSystemWrite(cacheSystem, address, ID, 2, array);
	return 0;
}

/*
	A function used to write a word to a specific cache in a cache system.
	Takes in a cache system, an address, an ID, and data for the cache which
	will be written to. Returns 0 if the write is successful and otherwise
	returns -1.
*/
int cacheSystemWordWrite(cacheSystem_t* cacheSystem, uint32_t address, uint8_t ID, uint32_t data) {
	/* Error Checking??*/
	if (cacheSystem == NULL || validAddresses(address, (uint32_t) 4) != 1 || (address % 4 != 0) || getCacheFromID(cacheSystem, ID) == NULL) {
		return -1;
	}
	if (cacheSystem->blockDataSize < 4) {
		cacheSystemByteWrite(cacheSystem, address, ID, (uint8_t) (data >> 16));
		cacheSystemByteWrite(cacheSystem, address + 2, ID, (uint8_t) (data & UINT16_MAX));
	}
	uint8_t array[4];
	array[0] = (uint8_t) (data >> 24);
	array[1] = (uint8_t) ((data >> 16) & UINT8_MAX);
	array[2] = (uint8_t) ((data >> 8) & UINT8_MAX);
	array[3] = (uint8_t) (data & UINT8_MAX);
	cacheSystemWrite(cacheSystem, address, ID, 4, array);
	return 0;
}

/*
	A function used to write a doubleword to a specific cache in a cache system.
	Takes in a cache system, an address, an ID, and data for the cache which
	will be written to. Returns 0 if the write is successful and otherwise
	returns -1.
*/
int cacheSystemDoubleWordWrite(cacheSystem_t* cacheSystem, uint32_t address, uint8_t ID, uint64_t data) {
	/* Error Checking??*/
	if (cacheSystem == NULL || validAddresses(address, (uint32_t) 8) != 1 || (address % 8 != 0) || getCacheFromID(cacheSystem, ID) == NULL) {
		return -1;
	}
	if (cacheSystem->blockDataSize < 8) {
		cacheSystemByteWrite(cacheSystem, address, ID, (uint8_t) (data >> 32));
		cacheSystemByteWrite(cacheSystem, address + 4, ID, (uint8_t) (data & UINT32_MAX));
	}
	uint8_t array[8];
	array[0] = (uint8_t) (data >> 56);
	array[1] = (uint8_t) ((data >> 48) & UINT8_MAX);
	array[2] = (uint8_t) ((data >> 40) & UINT8_MAX);
	array[3] = (uint8_t) ((data >> 32) & UINT8_MAX);
	array[4] = (uint8_t) ((data >> 24) & UINT8_MAX);
	array[5] = (uint8_t) ((data >> 16) & UINT8_MAX);
	array[6] = (uint8_t) ((data >> 8) & UINT8_MAX);
	array[7] = (uint8_t) (data & UINT8_MAX);
	cacheSystemWrite(cacheSystem, address, ID, 8, array);
	return 0;
}
