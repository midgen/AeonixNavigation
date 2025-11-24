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

    /** Maximum distance from origin to flood fill. Points farther than this radius will not be generated. */
    UPROPERTY(EditDefaultsOnly, Category = Generator)
    FAIDataProviderFloatValue FloodRadius;

    /** Maximum number of steps to explore during flood fill. Stops when either radius or step limit is reached. */
    UPROPERTY(EditDefaultsOnly, Category = Generator)
    FAIDataProviderIntValue FloodStepsMax;

    // Optionally restrict to a specific navigation agent
    UPROPERTY(EditDefaultsOnly, Category = Generator)
    FAIDataProviderIntValue NavAgentIndex;

	/** Minimum spacing between generated points (0 = no filtering, returns all navigable points) */
	UPROPERTY(EditDefaultsOnly, Category = Generator)
	FAIDataProviderFloatValue MinPointSpacing;

	/** context */
	UPROPERTY(EditAnywhere, Category = Generator)
	TSubclassOf<class UEnvQueryContext> Context;

protected:
    virtual void GenerateItems(FEnvQueryInstance& QueryInstance) const override;
};
