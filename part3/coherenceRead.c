/* Summer 2017 */
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include "coherenceUtils.h"
#include "coherenceRead.h"
#include "../part1/utils.h"
#include "../part1/setInCache.h"
#include "../part1/getFromCache.h"
#include "../part1/cacheRead.h"
#include "../part1/cacheWrite.h"
#include "../part1/mem.h"
#include "../part2/hitRate.h"

/*
	A function which processes all cache reads for an entire cache system.
	Takes in a cache system, an address, an id for a cache, and a size to read
	and calls the appropriate functions on the cache being selected to read
	the data. Returns the data.
*/
uint8_t* cacheSystemRead(cacheSystem_t* cacheSystem, uint32_t address, uint8_t ID, uint8_t size) {
	uint8_t* retVal;
	uint8_t offset;
	uint8_t* transferData;
	evictionInfo_t* dstCacheInfo;
	evictionInfo_t* otherCacheInfo;
	uint32_t evictionBlockNumber;
	cacheNode_t** caches;
	bool otherCacheContains = false;
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
		retVal = readFromCache(dstCache, address, size);	// If it is in the cache, read it (read hit)
	} else {
		uint32_t oldAddress = extractAddress(dstCache, extractTag(dstCache, evictionBlockNumber), evictionBlockNumber, 0);
		/*How do you need to update the snooper?*/
		/*How do you need to update states for what is getting evicted
		(don't worry about evicting, this will be handled at a later step when you place data in the cache)?*/
		removeFromSnooper(cacheSystem->snooper, oldAddress, ID, size);	// Not necessary to keep track anymore
		//evict(dstCache, dstCacheInfo->blockNumber);
		//evict(dstCache, dstCacheInfo->blockNumber);
		//setState(dstCache,dstCacheInfo->blockNumber,INVALID);
		int otherID = returnIDIf1(cacheSystem->snooper, oldAddress, cacheSystem->blockDataSize);
		if (otherID != -1) { 														// If the other block is the only one that had the oldAddr
			updateState(getCacheFromID(cacheSystem, otherID), oldAddress, INVALID); // We update that state to MODIFIED/EXCLUSIVE
		}

		int val = returnFirstCacheID(cacheSystem->snooper, address, cacheSystem->blockDataSize);
		/*Check other caches???*/
		if (val != -1) { // ProbeRead
			otherCacheContains = 1;
			otherCacheInfo = findEviction(getCacheFromID(cacheSystem, val), address);					// Find block to be copied
			transferData = fetchBlock(getCacheFromID(cacheSystem, val), otherCacheInfo->blockNumber);	// Fetch the whole block
			//printCache(dstCache);
			writeWholeBlock(dstCache, address, evictionBlockNumber, transferData);
			updateState(getCacheFromID(cacheSystem, val), address, SHARED);
			setState(dstCache, evictionBlockNumber, SHARED);
			free(otherCacheInfo);
			free(transferData);
		}

		if (!otherCacheContains) { // Checks memory and set it to EXCLUSIVE
			/*Check Main memory?*/
			transferData = readFromMem(dstCache, address - offset);
			setData(dstCache,transferData,dstCacheInfo->blockNumber,dstCache->blockDataSize,0);
			setTag(dstCache,getTag(dstCache,address),dstCacheInfo->blockNumber);
			setState(dstCache, dstCacheInfo->blockNumber, EXCLUSIVE);
			free(transferData);
		}
		retVal = getData(dstCache, offset, evictionBlockNumber, size);
	}
	addToSnooper(cacheSystem->snooper, address, ID, cacheSystem->blockDataSize);
	if (otherCacheContains) {
		/*What states need to be updated?*/
		/*Your Code Here*/
		setState(dstCache, evictionBlockNumber, SHARED);
	}
	free(dstCacheInfo);
	return retVal;
}

/*
	A function used to request a byte from a specific cache in a cache system.
	Takes in a cache system, an address, and an ID for the cache which will be
	read from. Returns a struct with the data and a bool field indicating
	whether or not the read was a success.
*/
byteInfo_t cacheSystemByteRead(cacheSystem_t* cacheSystem, uint32_t address, uint8_t ID) {
	byteInfo_t retVal;
	uint8_t* data;
	/* Error Checking??*/
	retVal.success = (validAddresses(address, (uint32_t) 1)  && (address % 1 == 0) && cacheSystem != NULL && getCacheFromID(cacheSystem, ID) != NULL);
	retVal.data = 0;
	if (!retVal.success) {
		return retVal;
	}
	data = cacheSystemRead(cacheSystem, address, ID, 1);
	if (data == NULL) {
		return retVal;
	}
	retVal.data = data[0];
	free(data);
	return retVal;
}

/*
	A function used to request a halfword from a specific cache in a cache system.
	Takes in a cache system, an address, and an ID for the cache which will be
	read from. Returns a struct with the data and a bool field indicating
	whether or not the read was a success.
*/
halfWordInfo_t cacheSystemHalfWordRead(cacheSystem_t* cacheSystem, uint32_t address, uint8_t ID) {
	byteInfo_t temp;
	halfWordInfo_t retVal;
	uint8_t* data;
	retVal.success = (validAddresses(address, (uint32_t) 2)  && (address % 2 == 0) && cacheSystem != NULL && getCacheFromID(cacheSystem, ID) != NULL);
	retVal.data = 0;
	if (!retVal.success) {
		return retVal;
	}
	retVal.success = true;
	if (cacheSystem->blockDataSize < 2) {
		temp = cacheSystemByteRead(cacheSystem, address, ID);
		retVal.data = temp.data;
		temp = cacheSystemByteRead(cacheSystem, address + 1, ID);
		retVal.data = (retVal.data << 8) | temp.data;
		return retVal;
	}
	data = cacheSystemRead(cacheSystem, address, ID, 2);
	if (data == NULL) {
		return retVal;
	}
	retVal.data = data[0];
	retVal.data = (retVal.data << 8) | data[1];
	free(data);
	return retVal;
}


/*
	A function used to request a word from a specific cache in a cache system.
	Takes in a cache system, an address, and an ID for the cache which will be
	read from. Returns a struct with the data and a bool field indicating
	whether or not the read was a success.
*/
wordInfo_t cacheSystemWordRead(cacheSystem_t* cacheSystem, uint32_t address, uint8_t ID) {
	halfWordInfo_t temp;
	wordInfo_t retVal;
	uint8_t* data;
	retVal.success = (validAddresses(address, (uint32_t) 4)  && (address % 4 == 0) && cacheSystem != NULL && getCacheFromID(cacheSystem, ID) != NULL);
	retVal.data = 0;
	if (!retVal.success) {
		return retVal;
	}
	retVal.success = true;
	if (cacheSystem->blockDataSize < 4) {
		temp = cacheSystemHalfWordRead(cacheSystem, address, ID);
		retVal.data = temp.data;
		temp = cacheSystemHalfWordRead(cacheSystem, address + 2, ID);
		retVal.data = (retVal.data << 16) | temp.data;
		return retVal;
	}
	data = cacheSystemRead(cacheSystem, address, ID, 4);
	if (data == NULL) {
		return retVal;
	}
	retVal.data = data[0];
	retVal.data = (retVal.data << 8) | data[1];
	retVal.data = (retVal.data << 8) | data[2];
	retVal.data = (retVal.data << 8) | data[3];
	free(data);
	return retVal;
}

/*
	A function used to request a doubleword from a specific cache in a cache system.
	Takes in a cache system, an address, and an ID for the cache which will be
	read from. Returns a struct with the data and a bool field indicating
	whether or not the read was a success.
*/
doubleWordInfo_t cacheSystemDoubleWordRead(cacheSystem_t* cacheSystem, uint32_t address, uint8_t ID) {
	wordInfo_t temp;
	doubleWordInfo_t retVal;
	uint8_t* data;
	/* Error Checking??*/
	retVal.success = (validAddresses(address, (uint32_t) 8)  && (address % 8 == 0) && cacheSystem != NULL && getCacheFromID(cacheSystem, ID) != NULL);
	retVal.data = 0;
	if (!retVal.success) {
		return retVal;
	}
	retVal.success = true;
	if (cacheSystem->blockDataSize < 8) {
		temp = cacheSystemWordRead(cacheSystem, address, ID);
		retVal.data = temp.data;
		temp = cacheSystemWordRead(cacheSystem, address + 4, ID);
		retVal.data = (retVal.data << 32) | temp.data;
		return retVal;
	}
	data = cacheSystemRead(cacheSystem, address, ID, 8);
	if (data == NULL) {
		return retVal;
	}
	retVal.data = data[0];
	retVal.data = (retVal.data << 8) | data[1];
	retVal.data = (retVal.data << 8) | data[2];
	retVal.data = (retVal.data << 8) | data[3];
	retVal.data = (retVal.data << 8) | data[4];
	retVal.data = (retVal.data << 8) | data[5];
	retVal.data = (retVal.data << 8) | data[6];
	retVal.data = (retVal.data << 8) | data[7];
	free(data);
	return retVal;
}
