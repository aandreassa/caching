/* Summer 2017 */
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include "utils.h"
#include "setInCache.h"
#include "cacheRead.h"
#include "cacheWrite.h"
#include "getFromCache.h"
#include "mem.h"
#include "../part2/hitRate.h"

/*
	Takes in a cache and a block number and fetches that block of data,
	returning it in a uint8_t* pointer.
*/
uint8_t* fetchBlock(cache_t* cache, uint32_t blockNumber) {
	uint64_t location = getDataLocation(cache, blockNumber, 0);
	uint32_t length = cache->blockDataSize;
	uint8_t* data = malloc(sizeof(uint8_t) << log_2(length));
	if (data == NULL) {
		allocationFailed();
	}
	int shiftAmount = location & 7;
	uint64_t byteLoc = location >> 3;
	if (shiftAmount == 0) {
		for (uint32_t i = 0; i < length; i++) {
			data[i] = cache->contents[byteLoc + i];
		}
	} else {
		length = length << 3;
		data[0] = cache->contents[byteLoc] << shiftAmount;
		length -= (8 - shiftAmount);
		int displacement = 1;
		while (length > 7) {
			data[displacement - 1] = data[displacement - 1] | (cache->contents[byteLoc + displacement] >> (8 - shiftAmount));
			data[displacement] = cache->contents[byteLoc + displacement] << shiftAmount;
			displacement++;
			length -= 8;
		}
		data[displacement - 1] = data[displacement - 1] | (cache->contents[byteLoc + displacement] >> (8 - shiftAmount));
	}
	return data;
}

/*
	Takes in a cache, an address, and a dataSize and reads from the cache at
	that address the number of bytes indicated by the size. If the data block
	is already in the cache it retrieves the contents. If the contents are not
	in the cache it is read into a new slot and if necessary something is
	evicted.
*/
uint8_t* readFromCache(cache_t* cache, uint32_t address, uint32_t dataSize) {
	evictionInfo_t* blockInfo = findEviction(cache, address);
	uint32_t tag = getTag(cache, address);
	uint32_t idx = getIndex(cache, address);
	uint8_t* contents;
	reportAccess(cache);
	if (blockInfo->match == 0) {
		evict(cache, blockInfo->blockNumber);					// Evict block (update mem and stuff)
		uint8_t* data = readFromMem(cache, address - getOffset(cache, address));
		setValid(cache, blockInfo->blockNumber, (uint8_t) 1);
		setData(cache, data, blockInfo->blockNumber, cache->blockDataSize, 0);
		setDirty(cache, blockInfo->blockNumber, (uint8_t) 0);
		setTag(cache, tag, blockInfo->blockNumber);
		free(data);
	}
	else { // hit
		reportHit(cache);
	}
	contents = getData(cache, getOffset(cache, address), blockInfo->blockNumber, dataSize);
	updateLRU(cache, tag, idx, blockInfo->LRU);
	free(blockInfo);
	return contents;

}

/*
	Takes in a cache and an address and fetches a byte of data.
	Returns a struct containing a bool field of whether or not
	data was successfully read and the data. This field should be
	false only if there is an alignment error or there is an invalid
	address selected.
*/
byteInfo_t readByte(cache_t* cache, uint32_t address) {
	byteInfo_t retVal;
	retVal.success = (validAddresses(address, (uint32_t) 1) && cache != NULL);
	retVal.data = 0;
	if (!retVal.success) {
		return retVal;
	}
	uint8_t* data = readFromCache(cache, address, (uint32_t)1);
	retVal.data = *data;
	free(data);
	return retVal;
}

/*
	Takes in a cache and an address and fetches a halfword of data.
	Returns a struct containing a bool field of whether or not
	data was successfully read and the data. This field should be
	false only if there is an alignment error or there is an invalid
	address selected.
*/
halfWordInfo_t readHalfWord(cache_t* cache, uint32_t address) {
	halfWordInfo_t retVal;
	retVal.success = (validAddresses(address, (uint32_t) 2)  && (address % 2 == 0) && cache != NULL);
	retVal.data = 0;
	if (!retVal.success) {
		return retVal;
	}
	if (cache->blockDataSize < 2) {
		for(uint32_t i = 0; i < 2; i ++) {
			byteInfo_t byteRead = readByte(cache, address + i);
			retVal.data = retVal.data << 8 | byteRead.data;
		}
		return retVal;
	}
	uint8_t* data = readFromCache(cache, address, (uint32_t)2);
	retVal.data = (*data << 8) | *(data + 1);
	free(data);
	return retVal;
}

/*
	Takes in a cache and an address and fetches a word of data.
	Returns a struct containing a bool field of whether or not
	data was successfully read and the data. This field should be
	false only if there is an alignment error or there is an invalid
	address selected.
*/
wordInfo_t readWord(cache_t* cache, uint32_t address) {
	wordInfo_t retVal;
	retVal.success = (validAddresses(address, (uint32_t) 4)  && (address % 4 == 0) && cache != NULL);
	retVal.data = 0;
	if (!retVal.success) {
		return retVal;
	}
	if (cache->blockDataSize < 4) {
		for(uint32_t i = 0; i < 2; i ++) {
			halfWordInfo_t halfWordRead = readHalfWord(cache, address + (2*i));
			retVal.data = retVal.data << 16 | halfWordRead.data;
		}
		return retVal;
	}

	uint8_t* data = readFromCache(cache, address, (uint32_t)4);
	for (int i = 0; i < 4; i++) {
		retVal.data = (retVal.data << 8) | *(data + i);
	}
	free(data);
	return retVal;
}

/*
	Takes in a cache and an address and fetches a double word of data.
	Returns a struct containing a bool field of whether or not
	data was successfully read and the data. This field should be
	false only if there is an alignment error or there is an invalid
	address selected.
*/
doubleWordInfo_t readDoubleWord(cache_t* cache, uint32_t address) {
	doubleWordInfo_t retVal;
	retVal.success = (validAddresses(address, (uint32_t) 8)  && (address % 8 == 0) && cache != NULL);
	retVal.data = 0;
	if (!retVal.success) {
		return retVal;
	}
	if (cache->blockDataSize < 8) {
		for(uint32_t i = 0; i < 2; i ++) {
			wordInfo_t wordRead = readWord(cache, address + (4*i));
			retVal.data = retVal.data << 32 | wordRead.data;
		}
		return retVal;
	}
	uint8_t* data = readFromCache(cache, address, (uint32_t)8);
	for (int i = 0; i < 8; i++) {
		retVal.data = (retVal.data << 8) | *(data + i);
	}
	free(data);
	return retVal;
}
