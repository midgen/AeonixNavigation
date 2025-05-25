#pragma once

#include "CoreMinimal.h"
#include "DataProviders/AIDataProvider.h"
#include "EnvironmentQuery/EnvQueryGenerator.h"
#include "EnvironmentQuery/EnvQueryTypes.h"
#include "AeonixEQSFloodFillGenerator.generated.h"

/**
 * EQS Generator that flood fills valid points from a given origin using Aeonix octree navigation data
 */
UCLASS(EditInlineNew, Category = "Aeonix|EQS")
class AEONIXNAVIGATION_API UAeonixEQSFloodFillGenerator : public UEnvQueryGenerator
{
    GENERATED_BODY()
public:
    UAeonixEQSFloodFillGenerator(const FObjectInitializer& ObjectInitialize);

    // The point to start the flood fill from
    UPROPERTY(EditDefaultsOnly, Category = Generator)
    FAIDataProviderFloatValue FloodRadius;

    // Optionally restrict to a specific navigation agent
    UPROPERTY(EditDefaultsOnly, Category = Generator)
    FAIDataProviderIntValue NavAgentIndex;

	/** context */
	UPROPERTY(EditAnywhere, Category = Generator)
	TSubclassOf<class UEnvQueryContext> Context;

protected:
    virtual void GenerateItems(FEnvQueryInstance& QueryInstance) const override;
};
