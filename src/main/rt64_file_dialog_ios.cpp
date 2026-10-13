#include "gui/rt64_file_dialog.h"
#include "zelda_support.h"
namespace RT64 {
std::atomic<bool> FileDialog::isOpen = false;
void FileDialog::initialize() {}
void FileDialog::finish() {}
std::filesystem::path FileDialog::getDirectoryPath() { return {}; }
std::filesystem::path FileDialog::getOpenFilename(const std::vector<FileFilter>&) {
    zelda64::show_error_message_box("RT64", "Use the game launcher to import files on iOS.");
    return {};
}
std::filesystem::path FileDialog::getSaveFilename(const std::vector<FileFilter>&) { return {}; }
}
