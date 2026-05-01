#pragma once

#include "CoreMinimal.h"

using FOnHttpResponse = TFunction<void(bool bSuccess, const FString& Body, int32 StatusCode)>;

class IDataBridgeHttpClient
{
public:
	virtual ~IDataBridgeHttpClient() = default;

	virtual void Get(const FString& URL, FOnHttpResponse Callback) = 0;
};
