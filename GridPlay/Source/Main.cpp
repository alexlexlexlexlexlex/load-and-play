// Grid Play : l'application et sa fenêtre.

#include "MainComponent.h"

#include <cstdio>
#include <cstdlib>

namespace gridplay
{
class GridPlayApplication : public juce::JUCEApplication
{
public:
    const juce::String getApplicationName() override       { return "Grid Play"; }
    const juce::String getApplicationVersion() override    { return JUCE_APPLICATION_VERSION_STRING; }
    bool moreThanOneInstanceAllowed() override             { return true; }

    void initialise (const juce::String& commandLine) override
    {
        window = std::make_unique<MainWindow>();

        // Outil de développement : GridPlay --snapshot image.png morceau.gridplay
        auto args = juce::StringArray::fromTokens (commandLine, true);
        if (args.size() >= 3 && args[0] == "--snapshot")
        {
            snapshotMode = true;
            window->content().openFile (juce::File (args[2].unquoted()));
            window->content().snapshot (juce::File (args[1].unquoted()), [] { juce::JUCEApplication::quit(); });
            return;
        }
        openFromCommandLine (commandLine);
    }

    void shutdown() override
    {
        // Ferme tout proprement : morceau, instruments, carte son, ports MIDI, réglages enregistrés.
        window.reset();

        // Puis quitte sans exécuter le nettoyage final des bibliothèques chargées. Certains
        // instruments (vu avec ceux de Steinberg) plantent dans ce nettoyage, ce qui affichait
        // « Grid Play a quitté de manière imprévue » alors que tout était déjà fermé.
        std::fflush (nullptr);
        std::_Exit (0);
    }

    void systemRequestedQuit() override
    {
        if (window != nullptr)
            window->content().requestQuit ([] { juce::JUCEApplication::quit(); });
        else
            quit();
    }

    // Double-clic sur un fichier .gridplay alors que l'appli est ouverte.
    // (macOS envoie aussi ici les fichiers passés au lancement : ignorés pendant une capture.)
    void anotherInstanceStarted (const juce::String& commandLine) override
    {
        if (! snapshotMode)
            openFromCommandLine (commandLine);
    }

private:
    void openFromCommandLine (const juce::String& commandLine)
    {
        const auto path = commandLine.unquoted().trim();
        if (path.isNotEmpty() && juce::File::isAbsolutePath (path) && juce::File (path).existsAsFile() && window != nullptr)
            window->content().openFile (juce::File (path));
    }

    class MainWindow : public juce::DocumentWindow
    {
    public:
        MainWindow() : juce::DocumentWindow ("Grid Play", juce::Colour (0xff1c1c1c), juce::DocumentWindow::allButtons)
        {
            setUsingNativeTitleBar (true);
            auto* main = new MainComponent();
            setContentOwned (main, true);
            main->onTitleChanged = [this, main] { setName (main->getWindowTitle()); };
           #if JUCE_MAC
            juce::MenuBarModel::setMacMainMenu (main);
           #else
            setMenuBar (main);
           #endif
            setResizable (true, true);
            setResizeLimits (900, 560, 10000, 10000);
            centreWithSize (getWidth(), getHeight());
            setVisible (true);
        }

        ~MainWindow() override
        {
           #if JUCE_MAC
            juce::MenuBarModel::setMacMainMenu (nullptr);
           #else
            setMenuBar (nullptr);
           #endif
        }

        MainComponent& content() { return *static_cast<MainComponent*> (getContentComponent()); }

        void closeButtonPressed() override { juce::JUCEApplication::getInstance()->systemRequestedQuit(); }
    };

    std::unique_ptr<MainWindow> window;
    bool snapshotMode = false;
};
} // namespace gridplay

START_JUCE_APPLICATION (gridplay::GridPlayApplication)
