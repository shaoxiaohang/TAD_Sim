#pragma once

#include "CoreMinimal.h"
#include "HAL/PlatformFileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "google/protobuf/message.h"
#include "google/protobuf/text_format.h"
#include "google/protobuf/util/json_util.h"

#include <optional>
#include <string>
#include <string_view>

namespace util
{
template <class MessageType>
std::optional<MessageType> DecodeProtoFromText(const FString& ProtoText, bool AllowUnknownField = false)
{
    MessageType message;
    google::protobuf::TextFormat::Parser parser;
    parser.AllowUnknownField(AllowUnknownField);
    if (!parser.ParseFromString(TCHAR_TO_UTF8(*ProtoText), &message))
    {
        return std::nullopt;
    }
    return message;
}

template <typename MessageType>
std::optional<MessageType> DecodeProtoFromTextFile(const FString& FilePath, bool AllowUnknownField = false)
{
    if (!FPlatformFileManager::Get().GetPlatformFile().FileExists(*FilePath))
    {
        UE_LOG(LogTemp, Warning, TEXT("File not exist: %s"), *FilePath);
        return std::nullopt;
    }
    FString FileContent;
    if (!FFileHelper::LoadFileToString(FileContent, *FilePath))
    {
        UE_LOG(LogTemp, Error, TEXT("Failed to read file: %s"), *FilePath);
        return std::nullopt;
    }
    else
    {
        return DecodeProtoFromText<MessageType>(FileContent, AllowUnknownField);
    }
    return std::nullopt;
}

}    // namespace util