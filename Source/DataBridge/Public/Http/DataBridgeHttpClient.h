#pragma once

#include "CoreMinimal.h"
#include "Http/IDataBridgeHttpClient.h"

class DATABRIDGE_API FDataBridgeHttpClient : public IDataBridgeHttpClient
{
public:
	FDataBridgeHttpClient(float InTimeout, int32 InRetryCount, float InRetryDelay);

	virtual void Get(const FString& URL, FOnHttpResponse Callback) override;

private:
	void GetInternal(const FString& URL, FOnHttpResponse Callback, int32 RemainingRetries);

	float Timeout;
	int32 RetryCount;
	float RetryDelay;
};
