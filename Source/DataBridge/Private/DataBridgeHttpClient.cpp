#include "DataBridgeHttpClient.h"
#include "DataBridgeLog.h"
#include "HttpModule.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "Containers/Ticker.h"

FDataBridgeHttpClient::FDataBridgeHttpClient(float InTimeout, int32 InRetryCount, float InRetryDelay)
	: Timeout(InTimeout)
	, RetryCount(InRetryCount)
	, RetryDelay(InRetryDelay)
{
}

void FDataBridgeHttpClient::Get(const FString& URL, FOnHttpResponse Callback)
{
	GetInternal(URL, MoveTemp(Callback), RetryCount);
}

void FDataBridgeHttpClient::GetInternal(const FString& URL, FOnHttpResponse Callback, int32 RemainingRetries)
{
	TSharedRef<IHttpRequest> Request = FHttpModule::Get().CreateRequest();
	Request->SetURL(URL);
	Request->SetVerb(TEXT("GET"));
	Request->SetTimeout(Timeout);

	Request->OnProcessRequestComplete().BindLambda(
		[this, URL, Callback, RemainingRetries](FHttpRequestPtr, FHttpResponsePtr Response, bool bConnected)
		{
			const bool bNetworkFail = !bConnected || !Response.IsValid();
			const int32 StatusCode = Response.IsValid() ? Response->GetResponseCode() : 0;
			const bool bServerError = StatusCode >= 500;

			if ((bNetworkFail || bServerError) && RemainingRetries > 0)
			{
				UE_LOG(LogDataBridge, Warning, TEXT("Request failed (status %d), retrying... (%d left) URL: %s"),
					StatusCode, RemainingRetries, *URL);

				FTSTicker::GetCoreTicker().AddTicker(
					FTickerDelegate::CreateLambda([this, URL, Callback, RemainingRetries](float) -> bool
					{
						GetInternal(URL, Callback, RemainingRetries - 1);
						return false;
					}),
					RetryDelay);

				return;
			}

			if (bNetworkFail)
			{
				Callback(false, TEXT(""), StatusCode);
				return;
			}

			if (StatusCode < 200 || StatusCode >= 300)
			{
				UE_LOG(LogDataBridge, Warning, TEXT("HTTP %d for URL: %s"), StatusCode, *URL);
				Callback(false, Response->GetContentAsString(), StatusCode);
				return;
			}

			Callback(true, Response->GetContentAsString(), StatusCode);
		});

	Request->ProcessRequest();
}
