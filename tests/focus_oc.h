#import <Foundation/Foundation.h>

@interface OCAppFocus : NSObject <NSApplicationDelegate>
{
NSRunningApplication    *currentApp;
}
@property (retain) NSRunningApplication *currentApp;
@end

@implementation OCAppFocus
@synthesize currentApp;

- (id)init
{
if ((self = [super init]))
{
[[[NSWorkspace sharedWorkspace] notificationCenter] addObserver:self
        selector:@selector(activeAppDidChange:)
name:NSWorkspaceDidActivateApplicationNotification object:nil];
}
return self;
}
- (void)dealloc
{
[[[NSWorkspace sharedWorkspace] notificationCenter] removeObserver:self];
[super dealloc];
}
- (void)activeAppDidChange:(NSNotification *)notification
{
self.currentApp = [[notification userInfo] objectForKey:NSWorkspaceApplicationKey];

NSLog(@"App:      %@", [currentApp localizedName]);
NSLog(@"Bundle:   %@", [currentApp bundleIdentifier]);
NSLog(@"Exec Url: %@", [currentApp executableURL]);
NSLog(@"PID:      %d", [currentApp processIdentifier]);
}
@end