/*
 * Copyright (c) 2018, Intel Corporation.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <Uefi.h>
#include "ShowAcpiCommand.h"
#include <Debug.h>
#include <Types.h>
#include <Utility.h>
#include <NvmInterface.h>
#include <NvmTables.h>
#include "Common.h"
#include <ShowAcpi.h>

/* Command syntax definition */
struct Command showAcpiCommand =
{
  SHOW_VERB,                                                           //!< verb
  {                                                                    //!< options
    {VERBOSE_OPTION_SHORT, VERBOSE_OPTION, L"", L"", FALSE, ValueEmpty},
#ifdef OS_BUILD
    { OUTPUT_OPTION_SHORT, OUTPUT_OPTION, L"", OUTPUT_OPTION_HELP, FALSE, ValueRequired }
#else
    {L"", L"", L"", L"", FALSE, ValueOptional}
#endif
  },                        //!< options
  {{SYSTEM_TARGET, L"", SYSTEM_ACPI_TARGETS, TRUE, ValueOptional}},    //!< targets
  {{L"", L"", L"", FALSE, ValueOptional}},                             //!< properties
  L"Show the ACPI tables related to the DIMMs in the system.",//!< help
  showAcpi                                                             //!< run function
};

/*
 * Register the show acpi command
 */
EFI_STATUS registerShowAcpiCommand() {
  EFI_STATUS rc = EFI_SUCCESS;
  NVDIMM_ENTRY();

  rc = RegisterCommand(&showAcpiCommand);

  NVDIMM_EXIT_I64(rc);
  return rc;
}

/*
 * Execute the show acpi command
 */
EFI_STATUS showAcpi(struct Command *pCmd) {
  EFI_STATUS ReturnCode = EFI_SUCCESS;
  EFI_DCPMM_CONFIG_PROTOCOL *pNvmDimmConfigProtocol = NULL;
  ParsedFitHeader *pNFit = NULL;
  ParsedPcatHeader *pPcat = NULL;
  PMTT_TABLE *pPMTT = NULL;
  CHAR16 *pSystemTargetValue = NULL;
  CHAR16 **ppStringElements = NULL;
  UINT32 ElementsCount = 0;
  UINT8 ChosenAcpiSystem = AcpiUnknown;
  UINT16 Index = 0;
  NVDIMM_ENTRY();

  if (pCmd == NULL) {
    ReturnCode = EFI_INVALID_PARAMETER;
    Print(FORMAT_STR_NL, CLI_ERR_NO_COMMAND);
    goto Finish;
  }

  ReturnCode = OpenNvmDimmProtocol(gNvmDimmConfigProtocolGuid, (VOID **)&pNvmDimmConfigProtocol, NULL);
  if (EFI_ERROR(ReturnCode)) {
    Print(FORMAT_STR_NL, CLI_ERR_OPENING_CONFIG_PROTOCOL);
    ReturnCode = EFI_NOT_FOUND;
    goto Finish;
  }

  if (!ContainTarget(pCmd, SYSTEM_TARGET)) {
    ReturnCode = EFI_INVALID_PARAMETER;
    goto Finish;
  }

  pSystemTargetValue = GetTargetValue(pCmd, SYSTEM_TARGET);

  ppStringElements = StrSplit(pSystemTargetValue, L',', &ElementsCount);
  if (ppStringElements != NULL) {
    for (Index = 0; Index < ElementsCount; ++Index) {
      if (StrICmp(ppStringElements[Index], NFIT_TARGET_VALUE) == 0) {
        ChosenAcpiSystem |= AcpiNfit;
      } else if (StrICmp(ppStringElements[Index], PCAT_TARGET_VALUE) == 0) {
        ChosenAcpiSystem |= AcpiPcat;
      } else if (StrICmp(ppStringElements[Index], PMTT_TARGET_VALUE) == 0) {
        ChosenAcpiSystem |= AcpiPMTT;
      } else {
        Print(L"The provided system ACPI Table: " FORMAT_STR L" is not valid\n", pSystemTargetValue);
        ReturnCode = EFI_INVALID_PARAMETER;
        goto Finish;
      }
    }
  } else {
    /* If no target value is mentioned, then print all */
    ChosenAcpiSystem = AcpiAll;
  }

  if (ChosenAcpiSystem == AcpiAll || ChosenAcpiSystem == AcpiNfit) {
    ReturnCode = pNvmDimmConfigProtocol->GetAcpiNFit(pNvmDimmConfigProtocol, &pNFit);
    if (EFI_ERROR(ReturnCode) || pNFit == NULL) {
      Print(L"Error: Failed to find the NVDIMM Firmware Interface ACPI tables\n");
      ReturnCode = EFI_ABORTED;
    } else {
      Print(L"---NVDIMM Firmware Interface Table---\n");
      PrintNFit(pNFit);
    }
  }

  if (ChosenAcpiSystem == AcpiAll || ChosenAcpiSystem == AcpiPcat) {
    ReturnCode = pNvmDimmConfigProtocol->GetAcpiPcat(pNvmDimmConfigProtocol, &pPcat);
    if (EFI_ERROR(ReturnCode) || pPcat == NULL) {
      Print(L"Error: Failed to find the PCAT tables\n");
    } else {
      Print(L"---Platform Configurations Attributes Table---\n");
      PrintPcat(pPcat);
    }
  }

  if (ChosenAcpiSystem == AcpiAll || ChosenAcpiSystem == AcpiPMTT) {
    ReturnCode = pNvmDimmConfigProtocol->GetAcpiPMTT(pNvmDimmConfigProtocol, &pPMTT);
    if (ReturnCode == EFI_NOT_FOUND) {
      Print(L"PMTT table not found.\n");
      ReturnCode = EFI_SUCCESS;
    } else if (EFI_ERROR(ReturnCode)|| pPMTT == NULL) {
      Print(L"Error: Failed to find the PMTT tables\n");
    } else {
      Print(L"---Platform Memory Topology Table---\n");
      PrintPMTT(pPMTT);
      FREE_POOL_SAFE(pPMTT);
    }
  }

Finish:
  if (ppStringElements != NULL) {
    FreeStringArray(ppStringElements, ElementsCount);
  }
  NVDIMM_EXIT_I64(ReturnCode);
  return ReturnCode;
}
