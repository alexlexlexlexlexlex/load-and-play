// Transforme des pages HTML en PDF A4 avec le moteur web de macOS (WebKit), sans
// navigateur à installer. La page signale qu'elle est prête (polices et images
// chargées) en mettant son titre à « READY ».
//
// Compilation : clang -fobjc-arc -framework Cocoa -framework WebKit html2pdf.m -o html2pdf
// Utilisation : html2pdf <dossier autorisé> <page.html> <sortie.pdf> [<page.html> <sortie.pdf> …]

#import <Cocoa/Cocoa.h>
#import <WebKit/WebKit.h>

@interface Renderer : NSObject <WKNavigationDelegate>
@property (strong) NSURL* root;
@property (strong) NSMutableArray<NSArray<NSURL*>*>* jobs;
@property (strong) WKWebView* web;
@property (strong) NSWindow* window;
@property int waited;
@end

@implementation Renderer

- (instancetype) initWithRoot: (NSURL*) root jobs: (NSMutableArray*) jobs
{
    self = [super init];
    _root = root;
    _jobs = jobs;
    WKWebViewConfiguration* config = [WKWebViewConfiguration new];
    if (@available (macOS 13.3, *))
        config.preferences.shouldPrintBackgrounds = YES;
    NSRect frame = NSMakeRect (0, 0, 680, 1009);   // la zone imprimable d'une page A4 en pixels CSS (1 px = 0,75 pt)
    _web = [[WKWebView alloc] initWithFrame: frame configuration: config];
    _web.navigationDelegate = self;
    _window = [[NSWindow alloc] initWithContentRect: frame styleMask: NSWindowStyleMaskBorderless
                                            backing: NSBackingStoreBuffered defer: NO];
    _window.contentView = _web;
    return self;
}

- (void) next
{
    if (_jobs.count == 0) { [NSApp terminate: nil]; return; }
    _waited = 0;
    [_web loadFileURL: _jobs[0][0] allowingReadAccessToURL: _root];
}

- (void) webView: (WKWebView*) webView didFinishNavigation: (WKNavigation*) navigation { [self waitUntilReady]; }

- (void) fail: (NSError*) error
{
    fprintf (stderr, "Erreur : %s\n", error.localizedDescription.UTF8String);
    exit (1);
}
- (void) webView: (WKWebView*) w didFailNavigation: (WKNavigation*) n withError: (NSError*) e { [self fail: e]; }
- (void) webView: (WKWebView*) w didFailProvisionalNavigation: (WKNavigation*) n withError: (NSError*) e { [self fail: e]; }

- (void) waitUntilReady
{
    [_web evaluateJavaScript: @"document.title" completionHandler: ^(id result, NSError* error) {
        self.waited++;
        if ([result isEqual: @"READY"] || self.waited > 200)
        {
            if (self.waited > 200) printf ("(page pas tout à fait prête, on imprime quand même)\n");
            [self printPage];
        }
        else
            dispatch_after (dispatch_time (DISPATCH_TIME_NOW, 100 * NSEC_PER_MSEC), dispatch_get_main_queue(), ^{ [self waitUntilReady]; });
    }];
}

- (void) printPage
{
    NSURL* output = _jobs[0][1];
    [[NSFileManager defaultManager] removeItemAtURL: output error: nil];
    NSPrintInfo* info = [NSPrintInfo new];
    info.paperSize = NSMakeSize (595.28, 841.89);   // A4
    info.leftMargin = info.rightMargin = 42.5;     // 15 mm
    info.topMargin = 40;                           // 14 mm
    info.bottomMargin = 45;                        // 16 mm
    info.horizontalPagination = NSPrintingPaginationModeFit;
    info.verticalPagination = NSPrintingPaginationModeAutomatic;
    info.horizontallyCentered = NO;
    info.verticallyCentered = NO;
    info.jobDisposition = NSPrintSaveJob;
    info.dictionary[NSPrintJobSavingURL] = output;
    NSPrintOperation* operation = [_web printOperationWithPrintInfo: info];
    operation.showsPrintPanel = NO;
    operation.showsProgressPanel = NO;
    operation.view.frame = _web.bounds;
    [operation runOperationModalForWindow: _window delegate: self
                           didRunSelector: @selector (printOperationDidRun:success:contextInfo:) contextInfo: NULL];
}

- (void) printOperationDidRun: (NSPrintOperation*) operation success: (BOOL) success contextInfo: (void*) contextInfo
{
    printf ("%s%s\n", success ? "PDF : " : "ÉCHEC : ", _jobs[0][1].path.UTF8String);
    if (! success) exit (1);
    [_jobs removeObjectAtIndex: 0];
    [self next];
}

@end

int main (int argc, const char* argv[])
{
    @autoreleasepool
    {
        if (argc < 4 || (argc - 2) % 2 != 0)
        {
            printf ("usage: html2pdf <dossier autorisé> <page.html> <sortie.pdf> [...]\n");
            return 1;
        }
        NSURL* root = [NSURL fileURLWithPath: @(argv[1]) isDirectory: YES];
        NSMutableArray* jobs = [NSMutableArray new];
        for (int i = 2; i + 1 < argc; i += 2)
            [jobs addObject: @[ [NSURL fileURLWithPath: @(argv[i])], [NSURL fileURLWithPath: @(argv[i + 1])] ]];

        NSApplication* app = [NSApplication sharedApplication];
        [app setActivationPolicy: NSApplicationActivationPolicyProhibited];
        Renderer* renderer = [[Renderer alloc] initWithRoot: root jobs: jobs];
        dispatch_async (dispatch_get_main_queue(), ^{ [renderer next]; });
        [app run];
    }
    return 0;
}
