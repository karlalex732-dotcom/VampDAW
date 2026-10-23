// Only the separately installed helper is executed. No runtime EXE extraction,
// download or search through PATH, CWD or plug-in folders.
fs::path audioEncoderPath(){
    static std::mutex mutex;std::lock_guard<std::mutex>lock(mutex);
    auto binary=executableDirectory(),path=binary/L"tools/ffmpeg/bin/ffmpeg.exe";
    if(_wcsicmp(binary.filename().c_str(),L"build")==0)path=binary.parent_path()/L"tools/ffmpeg/bin/ffmpeg.exe";
    std::error_code ec;if(!fs::is_regular_file(path,ec))throw std::runtime_error("Audio encoder is missing. Reinstall the complete VampDAW package including tools/ffmpeg/bin/ffmpeg.exe.");
    HCRYPTPROV provider=0;HCRYPTHASH hash=0;
    if(!CryptAcquireContextW(&provider,nullptr,nullptr,PROV_RSA_AES,CRYPT_VERIFYCONTEXT))throw std::runtime_error("Cannot initialize encoder integrity verification");
    struct Cleanup{HCRYPTPROV p;HCRYPTHASH h=0;~Cleanup(){if(h)CryptDestroyHash(h);if(p)CryptReleaseContext(p,0);}}cleanup{provider};
    if(!CryptCreateHash(provider,CALG_SHA_256,0,0,&hash))throw std::runtime_error("Cannot initialize encoder SHA-256");cleanup.h=hash;
    std::ifstream input(path,std::ios::binary);std::array<char,65536>block{};
    while(input){input.read(block.data(),block.size());auto count=input.gcount();if(count&&!CryptHashData(hash,(const BYTE*)block.data(),(DWORD)count,0))throw std::runtime_error("Cannot verify audio encoder");}
    if(!input.eof())throw std::runtime_error("Cannot read audio encoder");std::array<BYTE,32>digest{};DWORD length=32;
    if(!CryptGetHashParam(hash,HP_HASHVAL,digest.data(),&length,0)||length!=32)throw std::runtime_error("Cannot finish encoder SHA-256");
    std::string actual;for(BYTE byte:digest){actual+="0123456789abcdef"[byte>>4];actual+="0123456789abcdef"[byte&15];}
    if(actual!=VAMP_ENCODER_SHA256)throw std::runtime_error("Audio encoder integrity check failed. Reinstall VampDAW from the official installer.");return path;
}
