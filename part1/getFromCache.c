/* Summer 2017 */
#include <stdlib.h>
#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdint.h>
#include <omp.h>
#include "utils.h"
#include "getFromCache.h"

/*
	Takes in a cache and a blocknumber and returns that block's valid bit.
*/
uint8_t getValid(cache_t* cache, uint32_t blockNumber) {
	/* Your Code Here. */
	return getBit(cache, numGarbageBits(cache) + blockNumber * totalBlockBits(cache));
}

/*
	Takes in a cache and a blocknumber and returns that block's dirty bit.
*/
uint8_t getDirty(cache_t* cache, uint32_t blockNumber) {
	/* Your Code Here. */
	return getBit(cache, numGarbageBits(cache) + blockNumber * totalBlockBits(cache) + 1);
}

/*
	Takes in a cache and a blocknumber and returns that block's shared bit.
*/
uint8_t getShared(cache_t* cache, uint32_t blockNumber) {
	/* Your Code Here. */
	return getBit(cache, numGarbageBits(cache) + blockNumber * totalBlockBits(cache) + 2);
}

/*
	Takes in a cache and a location of the cache in bits and returns the
	value of the bit at that location.
*/
uint8_t getBit(cache_t* cache, uint64_t location) {
	/* Your Code Here. */
	uint8_t getByte = *(cache->contents + (location >> 3));
	uint8_t chooseBit = ((uint8_t)128) >> (location % 8);
	return oneBitOn(getByte & chooseBit);
}

/*
	Takes a cache and a block number and extracts the value of the LRU
	for the block specified.
*/
long getLRU(cache_t* cache, uint32_t blockNumber) {
	/* Your Code Here. */
	uint8_t LRUlen = numLRUBits(cache);
	long result = 0;
	//printf("blockNumber is %u\n", blockNumber);
	for (uint8_t i = 0; i < LRUlen; i++) {
		//printf("The %u bit is %u\n", i, getBit(cache, numGarbageBits(cache) + blockNumber * totalBlockBits(cache) + (uint64_t)3 + i));
		result = result | getBit(cache, numGarbageBits(cache) + blockNumber * totalBlockBits(cache) + (uint64_t)3 + (uint64_t) i);
		if (i == LRUlen - 1)
			break;
		result = result << 1;
	}
	// printf("Result is %d\n", result);
	return result;
}

/*
	Takes a cache and a block number and extracts the value of the tag
	for the block specified.
*/
uint32_t extractTag(cache_t* cache, uint32_t blockNumber) {
	uint64_t location = getTagLocation(cache, blockNumber);
	uint64_t byteLoc = location >> 3;
	int shiftAmount = location & 7;
	if (shiftAmount != 0) {
		uint64_t newTag = (((uint64_t)  cache->contents[byteLoc]) << 56) + (((uint64_t) cache->contents[byteLoc + 1]) << 48)
	 	+ (((uint64_t)  cache->contents[byteLoc + 2]) << 40) + (((uint64_t) cache->contents[byteLoc + 3]) << 32)
	 	+ (((uint64_t) cache->contents[byteLoc + 4]) << 24);
	 	newTag = (newTag  << (shiftAmount));
	 	newTag = newTag >> (64 - getTagSize(cache));
		return ((uint32_t) newTag);
	} else {
		uint32_t newTag = (((uint32_t)  cache->contents[byteLoc]) << 24) + ((uint32_t) cache->contents[byteLoc + 1] << 16)
	 	+ (((uint32_t)  cache->contents[byteLoc + 2]) << 8) + ((uint32_t) cache->contents[byteLoc + 3]);
	 	newTag = newTag >> (32 - getTagSize(cache));
	 	return newTag;
	}
}

/*
	Takes a cache and a block number and extracts the value of the index
	for the block specified.
*/
uint32_t extractIndex(cache_t* cache, uint32_t blockNumber) {
	return (uint32_t) (blockNumber >> log_2(cache->n));
}

/*
	Takes in a cache, a tag, a blocknumber, and an offset and extracts the
	original address.
*/
uint32_t extractAddress(cache_t* cache, uint32_t tag, uint32_t blockNumber, uint32_t offset) {
	/* Your Code Here. */

	uint32_t result = (extractTag(cache, blockNumber) << (32 - getTagSize(cache)));
	result = result | (extractIndex(cache, blockNumber) << log_2(cache->blockDataSize));
	result = result | offset;
	return result;
}

/*
	Takes in a cache and an address and finds the next block that should be
	used for a cache operation on the address provided. If this address is
	already in memory it should return the block at which this address's
	operation would occur and indicates that this was a successful match.
	If this address is not stored in the cache then it should point to the next
	block that needs to be evicted as indicated by our LRU replacement system.
	If there are multiple blocks that could be evicted selects the
	block that occurs earlier in the cache. Returns a pointer to a struct
	which contains a block number, an LRU value, and whether or not the address
	is already stored in the cache (is a match).
*/
evictionInfo_t* findEviction(cache_t* cache, uint32_t address) {
	evictionInfo_t* info;
	info = malloc(sizeof(evictionInfo_t));
	if (info == NULL) {
		allocationFailed();
		return NULL;
	}

	//uint32_t numBlocks = cache->totalDataSize / cache->blockDataSize;
	//uint32_t blocksPerSet = numBlocks / getNumSets(cache);
	uint32_t index = getIndex(cache, address);
	//uint32_t blockNumber = index * blocksPerSet;
	uint32_t blockNumber = index * cache->n;

	//printf("Size of LRU is %u\n", numLRUBits(cache));
	uint32_t highestLRU = getLRU(cache, blockNumber);
	uint32_t highestBlock = blockNumber;
	uint32_t tag = getTag(cache, address);
	for (uint32_t i = blockNumber; i < blockNumber + cache->n; i++) {
		if (tagEquals(i, tag, cache) && getValid(cache, i) == 1) {
			info->match = 1;
			info->LRU = getLRU(cache, i);
			info->blockNumber = i;
			return info;
		} else if (highestLRU < getLRU(cache, i)) {
			highestLRU = getLRU(cache, i);
			highestBlock = i;
		}
	}
	info->match = 0;
	info->LRU = highestLRU;
	info->blockNumber = highestBlock;
	return info;
}

/*
	Takes in a cache and an address and returns the LRU
	value of that address in the cache. Used mostly for testing.
	Returns -1 if the information is not present in the cache.
*/
long getLRUAddress(cache_t* cache, uint32_t address){
	uint32_t tag;
	uint32_t idx = getIndex(cache, address);
	long tempLRU;
	for (int i = 0; i < cache->n; i++) {
		tempLRU = getLRU(cache, (idx << log_2(cache->n)) + i);
		tag = getTag(cache, address);
		if (tagEquals((idx << log_2(cache->n)) + i, tag, cache)) {
			return tempLRU;
		}
	}
	return -1;
}

/*
	Takes in a cache, an address, a blocknumber, and a size and
	returns a pointer to an array of the data that was read. ASSUMES THAT
	ALL THE DATA FITS IN THE BLOCK. This should be handled by a function
	higher up that calls this function.
*/
uint8_t* getData(cache_t* cache, uint32_t offset, uint32_t blockNumber, uint32_t size) {
	uint8_t* data;
	uint8_t mask;
	uint64_t location = getDataLocation(cache, blockNumber, offset);
	uint64_t byteLoc = location >> 3;
	uint8_t shiftAmount = location & 7;
	data = (uint8_t*) malloc(sizeof(uint8_t) * size);
	if (data == NULL) {
		allocationFailed();
	}
	if (shiftAmount == 0) {
		for (uint32_t i = 0; i < size; i++) {
			data[i] = cache->contents[byteLoc + i];
		}
	} else {
		mask = 0;
		for (uint8_t j = 0; j < shiftAmount; j++) {
			mask += (1 << j);
		}
		for (uint32_t i = 0; i < size; i++) {
			data[i] = cache->contents[byteLoc + i] << shiftAmount;
			data[i] = data[i] | ((cache->contents[byteLoc + i + 1] >> (8 - shiftAmount)) & mask);
		}
	}
	return data;
}
