//
//  ZoomBlorbFile.h
//  ZoomCocoa
//
//  Created by Andrew Hunter on Fri Jul 30 2004.
//  Copyright (c) 2004 Andrew Hunter. All rights reserved.
//

#import <Foundation/Foundation.h>

#import "ZoomProtocol.h"

@interface ZoomBlorbFile : NSObject {
	NSObject<ZFile>* file;
	
	NSString*       formID;
	unsigned int    formLength;

	NSMutableArray*		 iffBlocks;
	NSMutableDictionary* typesToBlocks;
	NSMutableDictionary* locationsToBlocks;
}

// Testing files
+ (BOOL) dataIsBlorbFile: (NSData*) data;
+ (BOOL) fileContentsIsBlorb: (NSString*) filename;
+ (BOOL) zfileIsBlorb: (NSObject<ZFile>*) file;

// Initialisation
- (id) initWithZFile: (NSObject<ZFile>*) file; // Designated initialiser
- (id) initWithData: (NSData*) blorbFile;
- (id) initWithContentsOfFile: (NSString*) filename;

// Generic IFF data
- (NSArray*) chunksWithType: (NSString*) chunkType;

// Typed data
- (NSData*) imageDataWithNumber: (int) num;
- (NSData*) soundDataWithNumber: (int) num;

// Decoded data

@end
