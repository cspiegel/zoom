//
//  ZoomStory.h
//  ZoomCocoa
//
//  Created by Andrew Hunter on Tue Jan 13 2004.
//  Copyright (c) 2004 __MyCompanyName__. All rights reserved.
//

#import <Foundation/Foundation.h>

@class ZoomStoryID;
@interface ZoomStory : NSObject<NSCopying> {
	struct IFMDStory* story;
	BOOL   needsFreeing;
}

// Initialisation
- (id) init;								// New story
- (id) initWithStory: (struct IFMDStory*) story;   // Existing story (not freed)

- (struct IFMDStory*) story;
- (void) addID: (ZoomStoryID*) newID;

// Accessors
- (NSString*) title;
- (NSString*) headline;
- (NSString*) author;
- (NSString*) genre;
- (int)       year;
- (NSString*) group;
- (unsigned)  zarfian;
- (NSString*) teaser;
- (NSString*) comment;
- (float)     rating;

// Setting data
- (void) setTitle:    (NSString*) newTitle;
- (void) setHeadline: (NSString*) newHeadline;
- (void) setAuthor:   (NSString*) newAuthor;
- (void) setGenre:    (NSString*) genre;
- (void) setYear:     (int) year;
- (void) setGroup:    (NSString*) group;
- (void) setZarfian:  (unsigned) zarfian;
- (void) setTeaser:   (NSString*) teaser;
- (void) setComment:  (NSString*) comment;
- (void) setRating:   (float) rating;

@end
