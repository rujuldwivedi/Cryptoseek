#include <iostream>
#include <fstream>
#include <unordered_map>
#include <vector>
#include <string>
#include <filesystem>
#include <sstream>
#include <algorithm>
#include <set>
#include <cctype>

using namespace std;
namespace fs = filesystem;

const string DOCS_DIR = "documents";        // Directory containing documents to index
const string ENCRYPTED_INDEX = "encrypted.idx";  // Output index file name
const char XOR_KEY = 'K';                   // Simple XOR encryption key

string toLower(const string &str) 
{
    string out = str;
    transform(out.begin(), out.end(), out.begin(), [](unsigned char c){ return tolower(c); });
    return out;
}

// Now first we'll clean and normalize a token (word) by removing punctuation and lowercasing
string cleanToken(string word) 
{
    // Removing all punctuation characters
    word.erase(remove_if(word.begin(), word.end(), [](unsigned char c) 
    {
        return ispunct(c);
    }), word.end());

    // Converting to lowercase
    word = toLower(word);

    return word;
}

// Now we'll do simple XOR encryption/decryption
string xorEncryptDecrypt(const string &input) 
{
    string result = input;
    // XORing each character with the key
    for (char &c : result)
        c ^= XOR_KEY;
    return result;
}

// Then we'll build inverted index from documents in DOCS_DIR
unordered_map<string, vector<string>> buildIndex() 
{
    unordered_map<string, set<string>> tempIndex;  // Using set to avoid duplicate filenames
    
    // Processing each .txt file in documents directory
    for (const auto &entry : fs::directory_iterator(DOCS_DIR))
    {
        if (entry.path().extension() == ".txt") 
        {
            ifstream file(entry.path());
            string word;

            // Reading each word from file
            while (file >> word) 
            {
                string cleaned = cleanToken(word);
                if (!cleaned.empty()) 
                {
                    // Adding filename to word's posting list
                    tempIndex[cleaned].insert(entry.path().filename().string());
                }
            }
        }
    }

    // Converting sets to vectors for final index
    unordered_map<string, vector<string>> index;
    for (auto &[word, files] : tempIndex) 
        index[word] = vector<string>(files.begin(), files.end());

    return index;
}

// Now we'll save the encrypted index to file
void saveEncryptedIndex(const unordered_map<string, vector<string>> &index) 
{
    ofstream out(ENCRYPTED_INDEX, ios_base::binary);

    // Formatting each index entry as "word:file1,file2,"
    for (const auto &[word, files] : index) 
    {
        ostringstream entry;
        entry << word << ":";
        for (const auto &file : files)
            entry << file << ",";

        // Encrypting and writing the entry
        string encrypted = xorEncryptDecrypt(entry.str() + "\n");
        out << encrypted;
    }
    out.close();

    cout << "Index built and encrypted to '" << ENCRYPTED_INDEX << "'\n";
}

// Finally we'll load and decrypt index file
unordered_map<string, vector<string>> loadDecryptedIndex() 
{
    unordered_map<string, vector<string>> index;

    ifstream in(ENCRYPTED_INDEX, ios_base::binary);
    if (!in) 
    {
        cerr << "ERROR: Could not open index file\n";
        return index;
    }

    // Reading entire file content
    string encrypted_content((istreambuf_iterator<char>(in)), istreambuf_iterator<char>());
    in.close();

    // Decrypting the content
    string content = xorEncryptDecrypt(encrypted_content);

    // Parsing each line
    istringstream iss(content);
    string line;
    while (getline(iss, line)) 
    {
        size_t colon_pos = line.find(':');
        if (colon_pos == string::npos) 
            continue;  // Skipping malformed lines

        // Extracting and cleaning the word
        string word = line.substr(0, colon_pos);
        word.erase(remove_if(word.begin(), word.end(), 
            [](unsigned char c){ return isspace(c); }), word.end());

        // Processing comma-separated filenames
        string files_str = line.substr(colon_pos + 1);
        if (!files_str.empty() && files_str.back() == ',') 
            files_str.pop_back();  // Removing trailing comma

        vector<string> files;
        istringstream files_ss(files_str);
        string file;
        while (getline(files_ss, file, ',')) 
        {
            if (!file.empty()) 
                files.push_back(file);
        }

        // Adding to index if valid entry
        if (!word.empty() && !files.empty()) 
            index[word] = files;
    }

    return index;
}

// Ready! Now we can search for a word in the index
void searchWord(const string &query) 
{
    string q = cleanToken(query);  // Cleaning the search term
    auto index = loadDecryptedIndex();
    
    auto it = index.find(q);
    if (it != index.end()) 
    {
        // Displaying found documents
        cout << "Found '" << q << "' in:\n";
        for (const auto& file : it->second) 
            cout << " - " << file << "\n";
    } 
    else 
    {
        cout << "No results for '" << q << "'\n";
        
        // Finding and displaying suggestions
        vector<string> suggestions;
        for (const auto& [word, _] : index) 
        {
            if (word.find(q) != string::npos) 
                suggestions.push_back(word);
        }
        
        if (!suggestions.empty()) 
        {
            cout << "Did you mean:\n";
            for (const auto& suggestion : suggestions) 
                cout << " - " << suggestion << "\n";
        }
    }
}

int main(int argc, char *argv[]) 
{
    if (argc < 2) 
    {
        cerr << "Usage:\n" 
             << "  cryptoseek index\n" 
             << "  cryptoseek search <word> \n";
        return 1;
    }

    string cmd = argv[1];

    if (cmd == "index") 
    {
        // Building and saving index
        auto index = buildIndex();
        saveEncryptedIndex(index);
    } 
    else if (cmd == "search" && argc == 3) 
    {
        // Searching for a word
        searchWord(argv[2]);
    }
    else 
    {
        cerr << "Invalid command or arguments.\n";
    }

    return 0;
}